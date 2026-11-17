# tools (development branch, never merged)

Helper scripts used while developing the DKMS driver. This branch is an
orphan branch: it shares no history with `dev`/`master` and is never merged.

Use a script from any checkout without switching branches:

```sh
git show origin/tools:tools/compare-objs.sh > /tmp/compare-objs.sh && bash /tmp/compare-objs.sh OLD NEW
```

or keep a worktree: `git worktree add ../gchd-tools tools`.

| Script | Purpose |
|---|---|
| `tools/compare-objs.sh [--relaxed] [--alias old=new] OLD NEW` | Build `dkms/` at two revisions and compare machine code and string data. Proves a refactor is code-identical. |
| `tools/compare-mailbox-ops.py [--alias old=new] OLD NEW` | Compare the mailbox/USB operation stream of every function from preprocessed source. For idiom changes where machine code differs. |
| `tools/gen-transcoder-fields.py` | Regenerate `dkms/gchd_transcoder_fields.h` from `src/gchd_hardware.hpp` (read only). Run from a `dev` checkout. |

Scripts must be run from the root of a checkout that contains `dkms/`.
