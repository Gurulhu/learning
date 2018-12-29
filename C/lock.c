
#include <stdlib.h>
#include <stdio.h>
#include <string.h> //strcmp

#include <unistd.h> //close()
#include <sys/epoll.h>
#include <libudev.h>

/* udev context */ //Copied from libusb
static struct udev *udev_ctx = NULL;  //placeholder for udev context.
static int udev_monitor_fd = -1;  //placeholder for udev monitor file descriptor
static struct udev_monitor *udev_monitor = NULL; //placeholder for udev monitor

struct udev_enumerate *enumerate; //placeholder for udev enumerate "class"
struct udev_device *dev; //placeholder for udev device "class"

static int epoll_fd = -1; //placeholder for event poller file descriptor
struct epoll_event udev_epoll = { 0 }; //placeholder for udev event poller

static int debug = 0;
static int lock = 0;

char *idv;
char *idp;
char *ser;

int init(){
  //Create a new udev context
  udev_ctx = udev_new();

  if (!udev_ctx) {
    fprintf(stdout, "Could not create udev context");
    goto err;
  }

  //create udev monitor
  udev_monitor = udev_monitor_new_from_netlink( udev_ctx, "udev" );
  if (!udev_monitor) {
    fprintf(stdout, "Could not initialize udev monitor");
    goto err_free_ctx;
  }


  if ( udev_monitor_filter_add_match_subsystem_devtype( udev_monitor, "usb", "usb_device" ) ) {
    fprintf(stdout, "Could not initialize filter for \"USB\" subsystem (Monitor)");
    goto err_free_monitor;
  }

  if (udev_monitor_enable_receiving(udev_monitor)) {
    fprintf(stdout, "Failed to enable udev monitor");
    goto err_free_monitor;
  }

  udev_monitor_fd = udev_monitor_get_fd(udev_monitor);

  //Create new enumeration "class"
  enumerate = udev_enumerate_new(udev_ctx);

  if (!enumerate) {
    fprintf(stdout, "Could not create enumeration");
    goto err_free_monitor;
  }

  //udev_enumerate_add_match_subsystem(enumerate,"usb") wasn't added because it fetches DEVTYPE=usb_interfaces and it only floods our detection
  if (udev_enumerate_add_match_property(enumerate, "DEVTYPE", "usb_device") ) {
    fprintf(stdout, "Could not initialize filter for \"USB\" subsystem (Enumeration)");
    goto err_free_enum;
  }

  epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    fprintf(stdout, "Failed to create epoll");
    goto err_free_enum;
  }

  udev_epoll.events = EPOLLIN;
  udev_epoll.data.fd = udev_monitor_fd;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udev_monitor_fd, &udev_epoll) < 0 ) {
    fprintf(stdout, "Failed to add udev_monitor_fd to epoll");
    goto err_free_epoll;
  }

  return 0;

  err_free_epoll:
    close(epoll_fd);
    epoll_fd = -1;

  err_free_enum:
    udev_enumerate_unref(enumerate);

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

int check_for_presence(){
  //Create new list entry pointer
  //(Needed for udev_list_entry_foreach method to be used used as iterator.)
  struct udev_list_entry *entry;
  const char *sysattr_placeholder;

  udev_enumerate_scan_devices(enumerate);

  //runs through list checking for the right device
  //vvv This function (actually, a macro) is basically a wrapper for a for loop, btw. vvv
  udev_list_entry_foreach( entry, udev_enumerate_get_list_entry(enumerate) ) {

    const char *path = udev_list_entry_get_name(entry);
    dev = udev_device_new_from_syspath(udev_ctx, path);

    if(dev) {
      if(debug){
        fprintf(stdout, "-------------\n" );
        sysattr_placeholder = udev_device_get_sysattr_value(dev, "manufacturer");
        if (sysattr_placeholder) { fprintf(stdout, "Manufacturer: %s\n", sysattr_placeholder); }

        sysattr_placeholder = udev_device_get_sysattr_value(dev, "product");
        if (sysattr_placeholder) { fprintf(stdout, "Product: %s\n", sysattr_placeholder); }

        sysattr_placeholder = udev_device_get_sysattr_value(dev, "idVendor");
        if(sysattr_placeholder) { fprintf(stdout, "Vendor ID: %s\n", sysattr_placeholder); }

        sysattr_placeholder = udev_device_get_sysattr_value(dev, "idProduct");
        if(sysattr_placeholder) { fprintf(stdout, "Product ID: %s\n", sysattr_placeholder); }

        sysattr_placeholder = udev_device_get_sysattr_value(dev, "serial");
        if(sysattr_placeholder) { fprintf(stdout, "Serial: %s\n", sysattr_placeholder); }
      }

      sysattr_placeholder = udev_device_get_sysattr_value(dev, "idVendor");
      if (!sysattr_placeholder || strcmp(idv, sysattr_placeholder) != 0) {continue;}

      sysattr_placeholder = udev_device_get_sysattr_value(dev, "idProduct");
      if (!sysattr_placeholder || strcmp(idp, sysattr_placeholder) != 0) {continue;}

      sysattr_placeholder = udev_device_get_sysattr_value(dev, "serial");
      if (!sysattr_placeholder || strcmp(ser, sysattr_placeholder) != 0) {continue;}

      return 1;
    }

    udev_device_unref(dev);
  }

  return 0;
}

int change_lock( int found ){
  if(debug) { fprintf(stdout, "lock: %d found: %d\n", lock, found); }
  if (lock != found) { return 0; }
  lock = 1 - found;
  return 1;
}

void run_forever(){
  int fd_count;
  struct epoll_event events[1];

  struct udev_list_entry *entry;

  while(1) {
    int i = 0;

    change_lock(check_for_presence( idv, idp, ser ));

    if (lock) {
      //system("loginctl terminate-user ");
      system("loginctl lock-sessions");
    }else{
      system("loginctl unlock-sessions");
    }

    fd_count = epoll_wait(epoll_fd, events, 1, -1 );
    if (fd_count < 0) {
      fprintf(stdout, "Error receiving udev message: %m");
      continue;
    }

    for ( i = 0; i < fd_count; i++ ) {
      if (events[i].data.fd == udev_monitor_fd && events[i].events && EPOLLIN) {
        dev = udev_monitor_receive_device(udev_monitor);
        if (dev == NULL) { continue; }

        if (debug){
          fprintf(stdout, "\nAction: %s\n", udev_device_get_action(dev));
          fprintf(stdout, "Node: %s\n", udev_device_get_devnode(dev));
          fprintf(stdout, "Subsystem: %s\n", udev_device_get_subsystem(dev));
          fprintf(stdout, "Devtype: %s\n", udev_device_get_devtype(dev));
          udev_list_entry_foreach(entry, udev_device_get_sysattr_list_entry(dev)){
            fprintf(stdout, "%s: %s\n", udev_list_entry_get_name(entry), udev_device_get_sysattr_value(dev, udev_list_entry_get_name(entry)));
          }
          fprintf(stdout, "UID: %s%s%s\n", udev_device_get_sysattr_value(dev, "idVendor"), udev_device_get_sysattr_value(dev, "idProduct"), udev_device_get_sysattr_value(dev, "serial"));
        }
        udev_unref(udev_ctx);
      }
    }
  }
}

int main( int argc, char * argv[])
{
  if ( argc < 3 ) { return 1; }

  idv = argv[1];
  idp = argv[2];
  ser = argv[3];

  if ( init() ) { return 1; }

  change_lock(check_for_presence( idv, idp, ser ));

  if (lock) {
    //system("loginctl terminate-user ");
    system("loginctl lock-sessions");
  }else{
    system("loginctl unlock-sessions");
  }

  run_forever();

  cleanup:
    close(epoll_fd);
    epoll_fd = -1;

    udev_enumerate_unref(enumerate);

    udev_monitor_unref(udev_monitor);
    udev_monitor = NULL;
    udev_monitor_fd = -1;

    udev_unref(udev_ctx);

    udev_ctx = NULL;

    return 0;
}
