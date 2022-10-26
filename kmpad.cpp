// BUILD:   g++ kmpad.cpp -o kmpad
// USAGE:   kmpad KEYBOARD_DEVICE MOUSE_DEVICE MOUSE_ACCELERATION
// EXAMPLE: sudo ./kmpad /dev/input/event0 /dev/input/event9 100

#include <fcntl.h>
#include <linux/uinput.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include "keys.h"

const int MAX_ABS = 32767;
const int SLEEP_US = 100;
const double DEC_A = 0.999;
const double DEC_B = 0.01;

// send event to gamepad device
static inline void send_event(int gamepad_fd, int type, int cdde, int value)
{
  static input_event gamepad_event;
  memset(&gamepad_event, 0, sizeof(struct input_event));
  gamepad_event.type  = type;
  gamepad_event.code  = cdde;
  gamepad_event.value = value;
  if (write(gamepad_fd, &gamepad_event, sizeof(struct input_event)) == -1)
  {
    printf("Failed to write event.\n");
  }
}

// map button click to button click
static inline void but_clk(input_event in_ev, int out_fd, unsigned short in_key, unsigned short out_key)
{
  if (in_ev.code == in_key && in_ev.value != 2)
  {
    send_event(out_fd, EV_KEY, out_key, in_ev.value);
    send_event(out_fd, EV_SYN, 0, 0);
  }
}

// map button click to abs value
static inline void abs_clk(input_event in_ev, int out_fd, unsigned short key, unsigned short btn, int value)
{
  if (in_ev.code == key)
  {
    int v = (in_ev.value == 1 || in_ev.value == 2) ? value : 0;
    send_event(out_fd, EV_ABS, btn, v);
    send_event(out_fd, EV_SYN, 0, 0);
  }
}

// apply acceleration to current speed
static inline void abs_acc(input_event in_ev, unsigned short key, double max, double acc, double *val)
{
  if (in_ev.code == key)
  {
    *val = std::max(-max, std::min(max, *val + (in_ev.value * acc)));
  }
}

// apply deceleration and set abs value
static inline void abs_dec(int out_fd, unsigned short btn, double max, double dec_a, double dec_b, double *val)
{
  *val *= dec_a - (abs(*val) / max * dec_b);
  send_event(out_fd, EV_ABS, btn, (int)round(*val));
  send_event(out_fd, EV_SYN, 0, 0);
}

// configure input device
static int read_device(const char *dev, bool grab)
{
  int fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd == -1)
  {
    printf("Failed to open device %s.\n", dev);
    exit(1);
  }
  char name[256];
  if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) == -1)
  {
    printf("Failed to read device %s.\n", dev);
    exit(1);
  }
  if (grab) {
    if (ioctl(fd, EVIOCGRAB, 1) == -1)
    {
      printf("Failed to grab device %s.\n", dev);
      exit(1);
    }
  }
  printf("Reading device %s.\n", name);
  return fd;
}

// configure output device
static int write_device()
{
  const char *dev = "/dev/uinput";
  const char *name = "Gamepad";
  struct uinput_user_dev uidev;
  int fd = open(dev, O_WRONLY | O_NONBLOCK);
  if (fd == -1)
  {
    printf("Failed to open device %s.\n", dev);
    exit(1);
  }
  ioctl(fd, UI_SET_EVBIT,  EV_KEY);
  ioctl(fd, UI_SET_KEYBIT, BTN_A);
  ioctl(fd, UI_SET_KEYBIT, BTN_B);
  ioctl(fd, UI_SET_KEYBIT, BTN_X);
  ioctl(fd, UI_SET_KEYBIT, BTN_Y);
  ioctl(fd, UI_SET_KEYBIT, BTN_TL);
  ioctl(fd, UI_SET_KEYBIT, BTN_TR);
  ioctl(fd, UI_SET_KEYBIT, BTN_TL2);
  ioctl(fd, UI_SET_KEYBIT, BTN_TR2);
  ioctl(fd, UI_SET_KEYBIT, BTN_START);
  ioctl(fd, UI_SET_KEYBIT, BTN_SELECT);
  ioctl(fd, UI_SET_KEYBIT, BTN_THUMBL);
  ioctl(fd, UI_SET_KEYBIT, BTN_THUMBR);
  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_UP);
  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_DOWN);
  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_LEFT);
  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_RIGHT);
  ioctl(fd, UI_SET_EVBIT,  EV_ABS);
  ioctl(fd, UI_SET_ABSBIT, ABS_X);
  ioctl(fd, UI_SET_ABSBIT, ABS_Y);
  ioctl(fd, UI_SET_ABSBIT, ABS_RX);
  ioctl(fd, UI_SET_ABSBIT, ABS_RY);
  ioctl(fd, UI_SET_ABSBIT, ABS_TILT_X);
  ioctl(fd, UI_SET_ABSBIT, ABS_TILT_Y);
  memset(&uidev, 0, sizeof(uidev));
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, name);
  uidev.id.bustype      =  BUS_USB;
  uidev.id.vendor       =  0x3;
  uidev.id.product      =  0x3;
  uidev.id.version      =  2;
  uidev.absmax [ABS_X]  =  MAX_ABS;
  uidev.absmin [ABS_X]  = -MAX_ABS;
  uidev.absfuzz[ABS_X]  =  0;
  uidev.absflat[ABS_X]  =  15;
  uidev.absmax [ABS_Y]  =  MAX_ABS;
  uidev.absmin [ABS_Y]  = -MAX_ABS;
  uidev.absfuzz[ABS_Y]  =  0;
  uidev.absflat[ABS_Y]  =  15;
  uidev.absmax [ABS_RX] =  MAX_ABS;
  uidev.absmin [ABS_RX] = -MAX_ABS;
  uidev.absfuzz[ABS_RX] =  0;
  uidev.absflat[ABS_RX] =  0;
  uidev.absmax [ABS_RY] =  MAX_ABS;
  uidev.absmin [ABS_RY] = -MAX_ABS;
  uidev.absfuzz[ABS_RY] =  0;
  uidev.absflat[ABS_RY] =  0;
  if (write(fd, &uidev, sizeof(uidev)) == -1)
  {
    printf("Failed to write device %s.\n", dev);
    exit(1);
  }
  if (ioctl(fd, UI_DEV_CREATE) == -1)
  {
    printf("Failed to create device %s.\n", dev);
    exit(1);
  }
  printf("Writing device %s.\n", name);
  return fd;
}

int main(int argc, char *argv[])
{
  if (argc != 4) {
    printf("Usage: %s KEYBOARD_DEVICE MOUSE_DEVICE MOUSE_ACCELERATION\n", argv[0]);
    exit(1);
  }
  const char *keyboard_dev = argv[1];
  const char *mouse_dev = argv[2];
  const double mouse_acc = std::stod(argv[3]);

  int keyboard_fd = read_device(keyboard_dev, false);
  int mouse_fd    = read_device(mouse_dev,    true);
  int gamepad_fd  = write_device();

  double mouse_x = 0, mouse_y = 0;
  struct input_event keyboard_ev, mouse_ev;
  while (1)
  {
    while (read(keyboard_fd, &keyboard_ev, sizeof(keyboard_ev)) > 0)
    {
      but_clk(keyboard_ev, gamepad_fd, KEY_LEFTALT,   BTN_A);
      but_clk(keyboard_ev, gamepad_fd, KEY_SPACE,     BTN_B);
      but_clk(keyboard_ev, gamepad_fd, KEY_X,         BTN_X);
      but_clk(keyboard_ev, gamepad_fd, KEY_C,         BTN_Y);
      but_clk(keyboard_ev, gamepad_fd, KEY_LEFTSHIFT, BTN_THUMBL);
      but_clk(keyboard_ev, gamepad_fd, KEY_Q,         BTN_DPAD_LEFT);
      but_clk(keyboard_ev, gamepad_fd, KEY_E,         BTN_DPAD_RIGHT);
      but_clk(keyboard_ev, gamepad_fd, KEY_R,         BTN_DPAD_UP);
      but_clk(keyboard_ev, gamepad_fd, KEY_F,         BTN_DPAD_DOWN);
      but_clk(keyboard_ev, gamepad_fd, KEY_I,         BTN_SELECT);
      but_clk(keyboard_ev, gamepad_fd, KEY_O,         BTN_START);
      but_clk(keyboard_ev, gamepad_fd, KEY_P,         BTN_MODE);
      but_clk(keyboard_ev, gamepad_fd, KEY_LEFT,      BTN_DPAD_LEFT);
      but_clk(keyboard_ev, gamepad_fd, KEY_RIGHT,     BTN_DPAD_RIGHT);
      but_clk(keyboard_ev, gamepad_fd, KEY_UP,        BTN_DPAD_UP);
      but_clk(keyboard_ev, gamepad_fd, KEY_DOWN,      BTN_DPAD_DOWN);
      abs_clk(keyboard_ev, gamepad_fd, KEY_A,         ABS_X,          -MAX_ABS);
      abs_clk(keyboard_ev, gamepad_fd, KEY_D,         ABS_X,           MAX_ABS);
      abs_clk(keyboard_ev, gamepad_fd, KEY_W,         ABS_Y,          -MAX_ABS);
      abs_clk(keyboard_ev, gamepad_fd, KEY_S,         ABS_Y,           MAX_ABS);
    }
    while (read(mouse_fd, &mouse_ev, sizeof(mouse_ev)) > 0)
    {
      but_clk(mouse_ev, gamepad_fd, BTN_LEFT,   BTN_TR2);
      but_clk(mouse_ev, gamepad_fd, BTN_RIGHT,  BTN_TL2);
      but_clk(mouse_ev, gamepad_fd, BTN_MIDDLE, BTN_THUMBR);
      but_clk(mouse_ev, gamepad_fd, BTN_EXTRA,  BTN_TL);
      but_clk(mouse_ev, gamepad_fd, BTN_SIDE,   BTN_TR);
      abs_acc(mouse_ev, ABS_X, MAX_ABS, mouse_acc, &mouse_x);
      abs_acc(mouse_ev, ABS_Y, MAX_ABS, mouse_acc, &mouse_y);
    }
    abs_dec(gamepad_fd, ABS_RX, MAX_ABS, DEC_A, DEC_B, &mouse_x);
    abs_dec(gamepad_fd, ABS_RY, MAX_ABS, DEC_A, DEC_B, &mouse_y);
    usleep(SLEEP_US);
  }
  return 0;
}
