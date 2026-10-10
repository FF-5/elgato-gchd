// SPDX-License-Identifier: GPL-2.0
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "elgato_gchd.h"

#define GCHD_TRACE_DEFAULT_BYTES (16UL * 1024UL * 1024UL)
#define GCHD_TRACE_MAX_BYTES     (64UL * 1024UL * 1024UL)
#define GCHD_TRACE_MAX_TRANSFERS 65536U

static unsigned long trace_bytes = GCHD_TRACE_DEFAULT_BYTES;
module_param(trace_bytes, ulong, 0444);
MODULE_PARM_DESC(trace_bytes, "Maximum raw USB stream bytes retained per device (debug branch, default 16 MiB, max 64 MiB)");

struct gchd_trace_transfer {
	u64 timestamp_ns;
	u32 offset;
	u32 length;
};

struct gchd_trace {
	struct kref ref;
	struct mutex lock;
	struct dentry *dir;
	u8 *data;
	struct gchd_trace_transfer *transfers;
	size_t capacity;
	size_t used;
	unsigned int transfer_count;
	unsigned int transfer_capacity;
	bool enabled;
	bool truncated;
};

static struct dentry *gchd_trace_root;
static DEFINE_MUTEX(gchd_trace_root_lock);

static void gchd_trace_release(struct kref *ref)
{
	struct gchd_trace *t = container_of(ref, struct gchd_trace, ref);

	vfree(t->data);
	kvfree(t->transfers);
	kfree(t);
}

static void gchd_trace_put(struct gchd_trace *t)
{
	kref_put(&t->ref, gchd_trace_release);
}

static int gchd_trace_status_show(struct seq_file *s, void *unused)
{
	struct gchd_trace *t = s->private;

	mutex_lock(&t->lock);
	seq_printf(s, "enabled=%u\nbytes_captured=%zu\ncapacity_bytes=%zu\ntransfers_captured=%u\ntransfer_capacity=%u\ntruncated=%u\n",
		   t->enabled, t->used, t->capacity, t->transfer_count,
		   t->transfer_capacity, t->truncated);
	mutex_unlock(&t->lock);
	return 0;
}

static int gchd_trace_status_open(struct inode *inode, struct file *file)
{
	struct gchd_trace *t = inode->i_private;
	int r;

	kref_get(&t->ref);
	r = single_open(file, gchd_trace_status_show, t);
	if (r)
		gchd_trace_put(t);
	return r;
}

static int gchd_trace_status_release(struct inode *inode, struct file *file)
{
	struct seq_file *s = file->private_data;
	struct gchd_trace *t = s->private;
	int r = single_release(inode, file);

	gchd_trace_put(t);
	return r;
}

static const struct file_operations gchd_trace_status_fops = {
	.owner = THIS_MODULE,
	.open = gchd_trace_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = gchd_trace_status_release,
};

static void *gchd_trace_seq_start(struct seq_file *s, loff_t *pos)
{
	struct gchd_trace *t = s->private;

	mutex_lock(&t->lock);
	if (*pos >= t->transfer_count) {
		mutex_unlock(&t->lock);
		return NULL;
	}
	return &t->transfers[*pos];
}

static void *gchd_trace_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct gchd_trace *t = s->private;

	++*pos;
	if (*pos >= t->transfer_count)
		return NULL;
	return &t->transfers[*pos];
}

static void gchd_trace_seq_stop(struct seq_file *s, void *v)
{
	struct gchd_trace *t = s->private;

	mutex_unlock(&t->lock);
}

static int gchd_trace_seq_show(struct seq_file *s, void *v)
{
	struct gchd_trace *t = s->private;
	struct gchd_trace_transfer *x = v;
	unsigned int index = x - t->transfers;
	size_t i, end = (size_t)x->offset + x->length;

	seq_printf(s, "# transfer=%u timestamp_ns=%llu offset=%u length=%u\n",
		   index, (unsigned long long)x->timestamp_ns, x->offset,
		   x->length);
	for (i = x->offset; i < end; i += 16) {
		size_t j, n = min_t(size_t, 16, end - i);

		seq_printf(s, "%08zx:", i);
		for (j = 0; j < n; ++j)
			seq_printf(s, " %02x", t->data[i + j]);
		seq_putc(s, '\n');
	}
	return 0;
}

static const struct seq_operations gchd_trace_seq_ops = {
	.start = gchd_trace_seq_start,
	.next = gchd_trace_seq_next,
	.stop = gchd_trace_seq_stop,
	.show = gchd_trace_seq_show,
};

static int gchd_trace_data_open(struct inode *inode, struct file *file)
{
	struct gchd_trace *t = inode->i_private;
	struct seq_file *s;
	int r;

	kref_get(&t->ref);
	r = seq_open(file, &gchd_trace_seq_ops);
	if (r) {
		gchd_trace_put(t);
		return r;
	}
	s = file->private_data;
	s->private = t;
	return 0;
}

static int gchd_trace_data_release(struct inode *inode, struct file *file)
{
	struct seq_file *s = file->private_data;
	struct gchd_trace *t = s->private;
	int r = seq_release(inode, file);

	gchd_trace_put(t);
	return r;
}

static const struct file_operations gchd_trace_data_fops = {
	.owner = THIS_MODULE,
	.open = gchd_trace_data_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = gchd_trace_data_release,
};

static int gchd_trace_enable_open(struct inode *inode, struct file *file)
{
	struct gchd_trace *t = inode->i_private;

	kref_get(&t->ref);
	file->private_data = t;
	return 0;
}

static int gchd_trace_enable_release(struct inode *inode, struct file *file)
{
	gchd_trace_put(file->private_data);
	return 0;
}

static ssize_t gchd_trace_enable_read(struct file *file, char __user *buf,
				      size_t count, loff_t *ppos)
{
	struct gchd_trace *t = file->private_data;
	char value[4];
	int len;

	mutex_lock(&t->lock);
	len = scnprintf(value, sizeof(value), "%u\n", t->enabled);
	mutex_unlock(&t->lock);
	return simple_read_from_buffer(buf, count, ppos, value, len);
}

static ssize_t gchd_trace_enable_write(struct file *file,
				       const char __user *buf,
				       size_t count, loff_t *ppos)
{
	struct gchd_trace *t = file->private_data;
	char value[16];
	bool enabled;
	int r;

	if (!count || count >= sizeof(value))
		return -EINVAL;
	if (copy_from_user(value, buf, count))
		return -EFAULT;
	value[count] = '\0';
	r = kstrtobool(value, &enabled);
	if (r)
		return r;

	mutex_lock(&t->lock);
	t->enabled = enabled && !t->truncated &&
		     t->used < t->capacity &&
		     t->transfer_count < t->transfer_capacity;
	mutex_unlock(&t->lock);
	return count;
}

static const struct file_operations gchd_trace_enable_fops = {
	.owner = THIS_MODULE,
	.open = gchd_trace_enable_open,
	.read = gchd_trace_enable_read,
	.write = gchd_trace_enable_write,
	.llseek = default_llseek,
	.release = gchd_trace_enable_release,
};

static int gchd_trace_clear_open(struct inode *inode, struct file *file)
{
	struct gchd_trace *t = inode->i_private;

	kref_get(&t->ref);
	file->private_data = t;
	return 0;
}

static int gchd_trace_clear_release(struct inode *inode, struct file *file)
{
	gchd_trace_put(file->private_data);
	return 0;
}

static ssize_t gchd_trace_clear_write(struct file *file,
				      const char __user *buf,
				      size_t count, loff_t *ppos)
{
	struct gchd_trace *t = file->private_data;
	char value[8];

	if (!count || count >= sizeof(value))
		return -EINVAL;
	if (copy_from_user(value, buf, count))
		return -EFAULT;
	value[count] = '\0';
	if (value[0] != '1' && value[0] != 'y' && value[0] != 'Y')
		return -EINVAL;

	mutex_lock(&t->lock);
	t->used = 0;
	t->transfer_count = 0;
	t->truncated = false;
	mutex_unlock(&t->lock);
	return count;
}

static const struct file_operations gchd_trace_clear_fops = {
	.owner = THIS_MODULE,
	.open = gchd_trace_clear_open,
	.write = gchd_trace_clear_write,
	.llseek = no_llseek,
	.release = gchd_trace_clear_release,
};

int gchd_trace_init(struct gchd *d)
{
	struct gchd_trace *t;
	int r = 0;

	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (!t)
		return -ENOMEM;
	kref_init(&t->ref);
	mutex_init(&t->lock);

	t->capacity = clamp_t(unsigned long, trace_bytes, 1024UL * 1024UL,
			      GCHD_TRACE_MAX_BYTES);
	t->transfer_capacity = min_t(size_t, GCHD_TRACE_MAX_TRANSFERS,
				      max_t(size_t, 4096, t->capacity / 256));
	t->data = vmalloc(t->capacity);
	t->transfers = kvmalloc_array(t->transfer_capacity,
				      sizeof(*t->transfers), GFP_KERNEL);
	if (!t->data || !t->transfers) {
		r = -ENOMEM;
		goto err;
	}

	mutex_lock(&gchd_trace_root_lock);
	if (!gchd_trace_root)
		gchd_trace_root = debugfs_create_dir("elgato_gchd", NULL);
	if (IS_ERR_OR_NULL(gchd_trace_root)) {
		r = -ENODEV;
		gchd_trace_root = NULL;
	} else {
		t->dir = debugfs_create_dir(dev_name(&d->intf->dev),
					    gchd_trace_root);
		if (IS_ERR_OR_NULL(t->dir)) {
			r = -ENODEV;
			t->dir = NULL;
		}
	}
	mutex_unlock(&gchd_trace_root_lock);
	if (r)
		goto err;

	debugfs_create_file("enable", 0600, t->dir, t,
			    &gchd_trace_enable_fops);
	debugfs_create_file("clear", 0200, t->dir, t,
			    &gchd_trace_clear_fops);
	debugfs_create_file("status", 0400, t->dir, t,
			    &gchd_trace_status_fops);
	debugfs_create_file("data", 0400, t->dir, t,
			    &gchd_trace_data_fops);
	d->trace = t;
	dev_info(&d->intf->dev,
		 "raw USB trace available at debugfs/elgato_gchd/%s (capacity=%zu bytes, initially disabled)\n",
		 dev_name(&d->intf->dev), t->capacity);
	return 0;

err:
	if (t->dir)
		debugfs_remove_recursive(t->dir);
	gchd_trace_put(t);
	return r;
}

void gchd_trace_destroy(struct gchd *d)
{
	struct gchd_trace *t = d->trace;

	if (!t)
		return;
	d->trace = NULL;
	mutex_lock(&t->lock);
	t->enabled = false;
	mutex_unlock(&t->lock);
	debugfs_remove_recursive(t->dir);
	t->dir = NULL;
	gchd_trace_put(t);
}

void gchd_trace_capture(struct gchd *d, const u8 *data, size_t len)
{
	struct gchd_trace *t = READ_ONCE(d->trace);
	struct gchd_trace_transfer *x;

	if (!t || !len || !READ_ONCE(t->enabled))
		return;

	mutex_lock(&t->lock);
	if (!t->enabled)
		goto out;
	if (len > t->capacity - t->used ||
	    t->transfer_count >= t->transfer_capacity) {
		t->truncated = true;
		t->enabled = false;
		goto out;
	}

	x = &t->transfers[t->transfer_count++];
	x->timestamp_ns = ktime_get_ns();
	x->offset = t->used;
	x->length = len;
	memcpy(t->data + t->used, data, len);
	t->used += len;

	if (t->used == t->capacity ||
	    t->transfer_count == t->transfer_capacity) {
		t->truncated = true;
		t->enabled = false;
	}
out:
	mutex_unlock(&t->lock);
}
