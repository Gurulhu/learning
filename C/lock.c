
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/epoll.h>
#include <libudev.h>

/* udev context */ //Copied from libusb
static struct udev *udev_ctx = NULL;
static int udev_monitor_fd = -1;
static int udev_control_pipe[2] = {-1, -1};
static struct udev_monitor *udev_monitor = NULL;

static void udev_hotplug_event(struct udev_device* udev_dev);
static void *linux_udev_event_thread_main(void *arg);

struct udev_enumerate *enumerate;
struct udev_device *dev;

int epoll_fd = -1;
struct epoll_event udev_epoll = { 0 };

int epoll_new(){
  epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    fprintf(stdout, "Failed to create epoll");
    return 1;
  }

  udev_epoll.events = EPOLLIN;
  udev_epoll.data.fd = udev_monitor_fd;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udev_monitor_fd, &udev_epoll) < 0 ) {
    fprintf(stdout, "Failed to add udev_monitor_fd to epoll");
    return 1;
  }

  return 0;
}

void monitor_and_print_forever(){
  while(1) {
    int fd_count;
    struct epoll_event events[1];
    int i = 0;

    fd_count = epoll_wait(epoll_fd, events, 1, -1 );
    if (fd_count < 0) {
      fprintf(stdout, "Error receiving udev message: %m");
      continue;
    }

    for ( i = 0; i < fd_count; i++ ) {
      if (events[i].data.fd == udev_monitor_fd && events[i].events && EPOLLIN) {
        dev = udev_monitor_receive_device(udev_monitor);
        struct udev_list_entry *entry;
        if (dev == NULL) { continue; }

        fprintf(stdout, "\nAction: %s\n", udev_device_get_action(dev));
        fprintf(stdout, "Node: %s\n", udev_device_get_devnode(dev));
        fprintf(stdout, "Subsystem: %s\n", udev_device_get_subsystem(dev));
        fprintf(stdout, "Devtype: %s\n", udev_device_get_devtype(dev));
        udev_list_entry_foreach(entry, udev_device_get_sysattr_list_entry(dev)){
          fprintf(stdout, "%s: %s\n", udev_list_entry_get_name(entry), udev_device_get_sysattr_value(dev, udev_list_entry_get_name(entry)));
        }
        fprintf(stdout, "UID: %s%s%s\n", udev_device_get_sysattr_value(dev, "idVendor"), udev_device_get_sysattr_value(dev, "idProduct"), udev_device_get_sysattr_value(dev, "serial"));
        udev_unref(udev_ctx);
      }
    }

  }
}


int main( int argc, char * argv[])
{
  int r;
  udev_ctx = NULL;
  udev_ctx = udev_new();

  if ( argc < 3 ) { return 1; }

  char *idv = argv[1];
  char *idp = argv[2];
  char *ser = argv[3];

  if (!udev_ctx) {
    fprintf(stdout, "Could not create udev context");
    goto err;
  }

  udev_monitor = udev_monitor_new_from_netlink( udev_ctx, "udev" );
  if (!udev_monitor) {
    fprintf(stdout, "Could not initialize udev monitor");
    goto err_free_ctx;
  }

  r = udev_monitor_filter_add_match_subsystem_devtype( udev_monitor, "usb", "usb_device" );
  if (r) {
    fprintf(stdout, "Could not initialize filter for \"USB\" subsystem");
    goto err_free_monitor;
  }

  if (udev_monitor_enable_receiving(udev_monitor)) {
    fprintf(stdout, "Failed to enable udev monitor");
    goto err_free_monitor;
  }

  udev_monitor_fd = udev_monitor_get_fd(udev_monitor);

  enumerate = udev_enumerate_new(udev_ctx);
  struct udev_list_entry *entry;

  udev_enumerate_add_match_subsystem(enumerate,"scsi");
  udev_enumerate_add_match_property(enumerate, "DEVTYPE", "scsi_device");
  udev_enumerate_scan_devices(enumerate);

  udev_list_entry_foreach( entry, udev_enumerate_get_list_entry(enumerate) ) {
    const char *path = udev_list_entry_get_name(entry);
    struct udev_device *scsi = udev_device_new_from_syspath(udev_ctx, path);

    struct udev_device *usb = udev_device_get_parent_with_subsystem_devtype(scsi, "usb", "usb_device");

    if(usb) {
      if (
        strcmp(idv, udev_device_get_sysattr_value(usb, "idVendor")) == 0 &&
        strcmp(idp, udev_device_get_sysattr_value(usb, "idProduct")) == 0 &&
        strcmp(ser, udev_device_get_sysattr_value(usb, "serial")) == 0
      ) { dev = usb; }
      
      fprintf(stdout, "Score: %d %d %d\n", strcmp(idv, udev_device_get_sysattr_value(usb, "idVendor")),
      strcmp(idp, udev_device_get_sysattr_value(usb, "idProduct")),
      strcmp(ser, udev_device_get_sysattr_value(usb, "serial")));
      fprintf(stdout, "UID: %s%s%s\n", udev_device_get_sysattr_value(usb, "idVendor"), udev_device_get_sysattr_value(usb, "idProduct"), udev_device_get_sysattr_value(usb, "serial"));
    }

    udev_device_unref(scsi);
  }

  udev_enumerate_unref(enumerate);

  if (dev != NULL){
    fprintf(stdout, "Deslockado!\n" );
  }

  cleanup:
    udev_monitor_unref(udev_monitor);
    udev_monitor = NULL;
    udev_monitor_fd = -1;

    udev_unref(udev_ctx);

    udev_ctx = NULL;

    return 0;

  err_free_monitor:
    udev_monitor_unref(udev_monitor);
    udev_monitor = NULL;
    udev_monitor_fd = -1;

  err_free_ctx:
    udev_unref(udev_ctx);

  err:
    udev_ctx = NULL;
    return 1;
}
