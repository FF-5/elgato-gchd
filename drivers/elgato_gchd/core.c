static int gchd_start(struct vb2_queue *q, unsigned int count)
{
 struct gchd *d = vb2_get_drv_priv(q);
 int r = 0;

 mutex_lock(&d->lifecycle_lock);
 if (d->disconnected) {
  r = -ENODEV;
  goto err;
 }

 /*
  * A file open is not proof that an application intends to capture:
  * udev's v4l_id and desktop media services open nodes to inspect them.
  * Defer all device boot/init and state-machine activity until STREAMON,
  * when a client has actually requested capture buffers to be streamed.
  */
 if (!READ_ONCE(d->hw_initialized)) {
  gchd_set_lifecycle(d, GCHD_INITIALIZATION);
  WRITE_ONCE(d->capture_active, false);
  d->input_configured = false;
  d->input_routed = false;
  d->input_detect_prepared = false;
  d->input_prepared = false;
  d->encoder_started = false;
  d->signal_present = false;
  d->input_width = 0;
  d->input_height = 0;
  d->input_fps_num = 0;
  d->input_fps_den = 0;

  dev_info(&d->intf->dev,
           "GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON\n");
  r = gchd_hw_init(d);
  if (r) {
   d->hw_initialized = false;
   d->hw_init_failed = true;
   gchd_set_lifecycle(d, GCHD_ERROR);
   goto err;
  }

  d->hw_initialized = true;
  d->hw_init_failed = false;
 }

 /* First actual capture request begins input selection and source detection. */
 if (!READ_ONCE(d->capture_active) &&
     !READ_ONCE(d->detect_requested)) {
  gchd_input_selection_enter(d);
 }
 WRITE_ONCE(d->streaming, true);
 if (!READ_ONCE(d->capture_active)) {
  WRITE_ONCE(d->detect_requested, true);
  gchd_set_lifecycle(d, GCHD_SOURCE_DETECTION);
  schedule_delayed_work(&d->detect_work, 0);
 }
 mutex_unlock(&d->lifecycle_lock);
 return 0;

err:
 mutex_unlock(&d->lifecycle_lock);
 gchd_return_queued(d, VB2_BUF_STATE_QUEUED);
 return r;
}

static void gchd_stop(struct vb2_queue *q)
{
 struct gchd *d = vb2_get_drv_priv(q);

 /* STREAMOFF stops buffer delivery, not hardware capture or source monitoring. */
 WRITE_ONCE(d->streaming, false);
 gchd_return_queued(d, VB2_BUF_STATE_ERROR);
}
static int gchd_fop_open(struct file *file)
{
 struct gchd *d = video_drvdata(file);
 int r = 0;

 /* Protect the embedded video_device/driver state against unplug races. */
 kref_get(&d->refcount);
 if (vb2_fops.open) {
  r = vb2_fops.open(file);
  if (r) {
   kref_put(&d->refcount, gchd_release_ref);
   return r;
  }
 }

 dev_info(&d->intf->dev,
          "GCHD lifecycle: V4L2 open requested by pid=%d comm=%s\n",
          task_pid_nr(current), current->comm);

 mutex_lock(&d->users_lock);
 if (d->disconnected) {
  r = -ENODEV;
 } else {
  /*
   * Opening the node only grants access to its V4L2 metadata and controls.
   * It must not boot hardware or start input detection: udev/v4l_id and
   * desktop media services routinely open nodes just to identify them.
   * The first STREAMON performs the ONLINE/POWER_OFF -> INITIALIZATION transition.
   */
  d->open_count++;
 }
 mutex_unlock(&d->users_lock);

 if (r && vb2_fops.release)
  vb2_fops.release(file);
 if (r)
  kref_put(&d->refcount, gchd_release_ref);
 return r;
}


