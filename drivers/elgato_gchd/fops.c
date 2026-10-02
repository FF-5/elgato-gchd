#include <linux/module.h>
#include "elgato_gchd.h"

const struct v4l2_file_operations vb2_fops = {
 .owner = THIS_MODULE,
 .open = v4l2_fh_open,
 .release = vb2_fop_release,
 .read = vb2_fop_read,
 .poll = vb2_fop_poll,
 .mmap = vb2_fop_mmap,
 .unlocked_ioctl = video_ioctl2,
};
EXPORT_SYMBOL_GPL(vb2_fops);
