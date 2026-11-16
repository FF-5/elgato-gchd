#!/usr/bin/env python3
"""Compare the mailbox/USB operation stream of dkms/ between two revisions.

tools/compare-objs.sh proves identity at machine-code level. Refactors that
replace one call-site idiom by another (local macros -> gchd_mailbox.h,
compound literals -> named arrays) are semantically identical but can change
register allocation and constant encoding, so the machine code differs. This
tool works one level higher:

1. preprocess every .c file with kbuild (make foo.i), at both revisions;
2. rewrite every known way of writing a mailbox operation into one canonical
   token, MW(port|b0,b1,...) for writes and MR(port|dest|len) for reads, with
   all integer constants evaluated;
3. drop the error plumbing (`r = ...; if (r) return r;`, GCHD_TRY wrappers,
   `do { } while (0)`), whitespace, and the names of temporary variables;
4. diff each function's normalised body.

Identical output means: same functions, same control flow, same mailbox
operations with the same bytes in the same order. Anything else is printed.

Usage: tools/compare-mailbox-ops.py [--alias old=new ...] [OLD_REV] [NEW_REV|WORKTREE]
  --alias  treat identifier `old` in OLD_REV as `new` (renamed helpers).
"""
import os, re, subprocess, sys, tempfile, glob, difflib

ALIAS = {}
args = sys.argv[1:]
while args and args[0] == '--alias':
    o, n = args[1].split('=')
    ALIAS[o] = n
    args = args[2:]
OLD = args[0] if len(args) > 0 else 'origin/dev'
NEW = args[1] if len(args) > 1 else 'WORKTREE'
TOP = subprocess.check_output(['git', 'rev-parse', '--show-toplevel'], text=True).strip()
KDIR = os.environ.get('KDIR') or '/lib/modules/%s/build' % os.uname().release
if not os.path.isdir(KDIR):
    c = sorted(glob.glob('/usr/src/linux-headers-*-generic'))
    KDIR = c[-1] if c else None
if not KDIR:
    sys.exit('no kernel headers; set KDIR')


def export(rev, dest):
    os.makedirs(dest)
    if rev == 'WORKTREE':
        subprocess.check_call(['cp', '-r', os.path.join(TOP, 'dkms') + '/.', dest])
    else:
        a = subprocess.Popen(['git', '-C', TOP, 'archive', rev, 'dkms'], stdout=subprocess.PIPE)
        subprocess.check_call(['tar', '-x', '-C', dest, '--strip-components=1'], stdin=a.stdout)
        a.wait()


def preprocess(src, work):
    # Same directory for both revisions so that __FILE__ etc. are equal.
    if os.path.exists(work):
        subprocess.check_call(['rm', '-rf', work])
    subprocess.check_call(['cp', '-r', src, work])
    out = {}
    for c in sorted(glob.glob(os.path.join(work, '*.c'))):
        if c.endswith('.mod.c'):
            continue
        base = os.path.basename(c)[:-2]
        subprocess.check_call(['make', '-s', '-C', KDIR, 'M=' + work, base + '.i'],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        text = open(os.path.join(work, base + '.i')).read()
        # keep only the part that comes from the driver's own .c file
        keep, cur = [], None
        for line in text.split('\n'):
            m = re.match(r'# \d+ "([^"]+)"', line)
            if m:
                cur = m.group(1)
                continue
            if cur and cur.endswith(base + '.c'):
                keep.append(line)
        out[base] = '\n'.join(keep)
    return out


def intval(tok):
    tok = tok.strip()
    while tok.startswith('(') and tok.endswith(')') and tok.count('(') == 1:
        tok = tok[1:-1].strip()
    tok = re.sub(r'(?i)^(0x[0-9a-f]+|\d+)[ul]*$', r'\1', tok)
    try:
        return str(int(tok, 0))
    except ValueError:
        return None


def norm_expr(e):
    e = re.sub(r'\s+', '', e)
    # strip redundant parentheses around simple tokens
    for _ in range(4):
        e = re.sub(r'\(([A-Za-z_][\w\->.\[\]]*|0x[0-9a-fA-F]+|\d+)\)', r'\1', e)
    e = re.sub(r'\b0x[0-9a-fA-F]+[uUlL]*\b', lambda m: str(int(re.sub(r'[uUlL]', '', m.group(0)), 16)), e)
    e = re.sub(r'\b(\d+)[uUlL]+\b', r'\1', e)
    return e


def bytes_list(s):
    parts = [p for p in re.split(r',(?![^()]*\))', s) if p.strip()]
    res = []
    for p in parts:
        v = intval(p)
        res.append(v if v is not None else norm_expr(p))
    return ','.join(res)


def canon(text):
    t = text
    # new form: do { u8 __gchd_b[] = { ... }; do { int __gchd_r = (gchd_mail_write((d), (P), __gchd_b, sizeof(__gchd_b))); if (__gchd_r) return __gchd_r; } while (0); } while (0)
    t = re.sub(r'do\s*\{\s*u8\s+(\w+)\s*\[\s*\]\s*=\s*\{([^{}]*)\}\s*;\s*do\s*\{\s*int\s+\w+\s*=\s*\(\s*gchd_mail_write\s*\(\s*\(?d\)?\s*,\s*\(?([^,()]+)\)?\s*,\s*\1\s*,\s*sizeof\s*\(\s*\1\s*\)\s*\)\s*\)\s*;\s*if\s*\(\s*\w+\s*\)\s*return\s+\w+\s*;\s*\}\s*while\s*\(\s*0\s*\)\s*;\s*\}\s*while\s*\(\s*0\s*\)',
               lambda m: ' MW(%s|%s) ' % (intval(m.group(3)) or norm_expr(m.group(3)), bytes_list(m.group(2))), t)
    # old local-macro form: do { u8 x[] = {..}; r = gchd_mail_write(d,P,x,N); if (r) return r; } while (0)
    t = re.sub(r'do\s*\{\s*u8\s+(\w+)\s*\[\s*\]\s*=\s*\{([^{}]*)\}\s*;\s*r\s*=\s*gchd_mail_write\s*\(\s*d\s*,\s*([^,]+?)\s*,\s*\1\s*,\s*\w+\s*\)\s*;\s*if\s*\(\s*r\s*\)\s*return\s+r\s*;\s*\}\s*while\s*\(\s*0\s*\)',
               lambda m: ' MW(%s|%s) ' % (intval(m.group(3)) or norm_expr(m.group(3)), bytes_list(m.group(2))), t)
    # old protocol_common form: do { u8 __b[]={..}; int __r=gchd_mail_write((d),(p),__b,sizeof(__b)); if(__r) return __r; } while(0)
    t = re.sub(r'do\s*\{\s*u8\s+(\w+)\s*\[\s*\]\s*=\s*\{([^{}]*)\}\s*;\s*int\s+(\w+)\s*=\s*gchd_mail_write\s*\(\s*\(?d\)?\s*,\s*\(?([^,()]+)\)?\s*,\s*\1\s*,\s*sizeof\s*\(\s*\1\s*\)\s*\)\s*;\s*if\s*\(\s*\3\s*\)\s*return\s+\3\s*;\s*\}\s*while\s*\(\s*0\s*\)',
               lambda m: ' MW(%s|%s) ' % (intval(m.group(4)) or norm_expr(m.group(4)), bytes_list(m.group(2))), t)
    # raw form: r = gchd_mail_write(d, P, (u8[]){..}, N); if (r) return r;
    t = re.sub(r'r\s*=\s*gchd_mail_write\s*\(\s*d\s*,\s*([^,]+?)\s*,\s*\(\s*u8\s*\[\s*\]\s*\)\s*\{([^{}]*)\}\s*,\s*\w+\s*\)\s*;\s*if\s*\(\s*r\s*\)\s*return\s+r\s*;',
               lambda m: ' MW(%s|%s) ' % (intval(m.group(1)) or norm_expr(m.group(1)), bytes_list(m.group(2))), t)
    # reads: new GCHD_TRY form and old forms
    t = re.sub(r'do\s*\{\s*int\s+\w+\s*=\s*\(\s*gchd_mail_read\s*\(\s*\(?d\)?\s*,\s*\(?([^,()]+)\)?\s*,\s*\(?(.+?)\)?\s*,\s*\(?(\w+)\)?\s*\)\s*\)\s*;\s*if\s*\(\s*\w+\s*\)\s*return\s+\w+\s*;\s*\}\s*while\s*\(\s*0\s*\)',
               lambda m: ' MR(%s|%s|%s) ' % (intval(m.group(1)) or norm_expr(m.group(1)), norm_expr(m.group(2)), intval(m.group(3)) or m.group(3)), t)
    t = re.sub(r'r\s*=\s*gchd_mail_read\s*\(\s*d\s*,\s*([^,]+?)\s*,\s*([^,]+?)\s*,\s*(\w+)\s*\)\s*;\s*if\s*\(\s*r\s*\)\s*return\s+r\s*;',
               lambda m: ' MR(%s|%s|%s) ' % (intval(m.group(1)) or norm_expr(m.group(1)), norm_expr(m.group(2)), intval(m.group(3)) or m.group(3)), t)
    # old GCHD_MR1: read into a temporary, then assign
    t = re.sub(r'u8\s+(\w+)\s*;\s*int\s+(\w+)\s*=\s*gchd_mail_read\s*\(\s*\(?d\)?\s*,\s*\(?(\w+)\)?\s*,\s*&\1\s*,\s*1\s*\)\s*;\s*if\s*\(\s*\2\s*\)\s*return\s+\2\s*;\s*\(?(\w+)\)?\s*=\s*\1\s*;',
               lambda m: ' MR(%s|&%s|1) ' % (intval(m.group(3)) or m.group(3), m.group(4)), t)
    # generic GCHD_TRY(expr) and `r = expr; if (r) return r;`
    t = re.sub(r'do\s*\{\s*int\s+(\w+)\s*=\s*(.*?)\s*;\s*if\s*\(\s*\1\s*\)\s*return\s+\1\s*;\s*\}\s*while\s*\(?\s*0\s*\)?', lambda m: ' TRY(%s) ' % strip_outer(m.group(2)), t)
    t = re.sub(r'TRY\(\s*gchd_mail_read\s*\(\s*\(?d\)?\s*,\s*\(?(\w+)\)?\s*,\s*\(?([^,()]+(?:\[[^\]]*\])?)\)?\s*,\s*\(?(\w+)\)?\s*\)\s*\)',
               lambda m: ' MR(%s|%s|%s) ' % (intval(m.group(1)) or m.group(1), norm_expr(m.group(2)), intval(m.group(3)) or m.group(3)), t)
    t = re.sub(r'\br\s*=\s*([^;]+?)\s*;\s*if\s*\(\s*r\s*\)\s*return\s+r\s*;', r' TRY(\1) ', t)
    # wrappers and noise: do { ... } while (0) carries no control flow
    t = re.sub(r'\}\s*while\s*\(?\s*0\s*\)?\s*;?', ' ', t)
    t = re.sub(r'\bdo\s*\{', ' ', t)
    for _ in range(3):
        t = re.sub(r'do\s*\{\s*((?:\s*(?:MW|MR|TRY)\([^()]*(?:\([^()]*\)[^()]*)*\)\s*;?)+)\s*\}\s*while\s*\(\s*0\s*\)', r' \1 ', t)
    t = re.sub(r'\s+', ' ', t)
    t = re.sub(r' ?; ?', ';', t)
    t = re.sub(r';+', ';', t)
    t = re.sub(r'\bint r;', '', t)
    t = re.sub(r'(MW|MR)\(([^)]*)\);?', lambda m: '\n' + m.group(0).rstrip(';') + '\n', t)
    t = norm_expr_numbers(t)
    for _ in range(3):
        t = re.sub(r'\(\s*(\d+)\s*\)', r'\1', t)
    t = re.sub(r'&(\w+)\[0\]', r'\1', t)            # &buf[0] == buf
    t = re.sub(r'\.lineno\s*=\s*\d+', '.lineno=N', t)  # dyndbg line numbers
    t = re.sub(r'__UNIQUE_ID_\w+?\d+', '__UNIQUE_ID', t)
    t = '\n'.join(l.strip() for l in t.split('\n') if l.strip())
    return t


def strip_outer(e):
    e = e.strip()
    if e.startswith('(') and e.endswith(')'):
        depth = 0
        for i, ch in enumerate(e):
            depth += ch == '('
            depth -= ch == ')'
            if depth == 0 and i < len(e) - 1:
                return re.sub(r'\s+', ' ', e)
        e = e[1:-1]
    e = re.sub(r'(?<=[(,])\s*\(\s*d\s*\)', 'd', e)
    return re.sub(r'\s+', ' ', e).replace(' ,', ',').replace(', ', ',')


def norm_expr_numbers(t):
    return re.sub(r'\b0x[0-9a-fA-F]+[uUlL]*\b', lambda m: str(int(re.sub(r'[uUlL]', '', m.group(0)), 16)), t)


def functions(text):
    """Split preprocessed C into {name: body} for top-level functions."""
    res, i, n = {}, 0, len(text)
    for m in re.finditer(r'\b(\w+)\s*\(([^;{}()]|\([^()]*\))*\)\s*\{', text):
        if m.start() < i:
            continue
        name = m.group(1)
        if name in ('if', 'while', 'for', 'switch'):
            continue
        depth, j = 0, m.end() - 1
        while j < n:
            if text[j] == '{':
                depth += 1
            elif text[j] == '}':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        res[name] = text[m.end():j]
        i = j
    return res


def main():
    work = tempfile.mkdtemp()
    try:
        export(OLD, os.path.join(work, 'old'))
        export(NEW, os.path.join(work, 'new'))
        a = preprocess(os.path.join(work, 'old'), os.path.join(work, 'b'))
        b = preprocess(os.path.join(work, 'new'), os.path.join(work, 'b'))
    finally:
        pass
    fa, fb = {}, {}
    for f, t in a.items():
        for o, n in ALIAS.items():
            t = re.sub(r'\b%s\b' % re.escape(o), n, t)
        fa.update(functions(t))
    for f, t in b.items():
        fb.update(functions(t))
    diffs = 0
    for name in sorted(set(fa) | set(fb)):
        if name not in fb:
            print('REMOVED function', name); diffs += 1; continue
        if name not in fa:
            print('ADDED function', name); diffs += 1; continue
        ca, cb = canon(fa[name]).split('\n'), canon(fb[name]).split('\n')
        if ca != cb:
            diffs += 1
            print('=== DIFFERS:', name)
            for l in difflib.unified_diff(ca, cb, lineterm='', n=1):
                print(l)
    subprocess.call(['rm', '-rf', work])
    if diffs == 0:
        print('IDENTICAL: %s vs %s perform the same mailbox operations in every function.' % (OLD, NEW))
    sys.exit(1 if diffs else 0)


main()
