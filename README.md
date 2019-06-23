# gpio-reflect
LKM for sending and receiving infrared signals (and others).

With this module it is possible to record and send signals by using hardware at the GPIO pins. These signals are interpreted protocol-independent, so any signal can be used.

The module recognizes over 50% of errors when recording signals. This error recognition can be customized with additional options.

Only one key press on a remote control is necessary to record the whole signal.

Once a signal is recorded it works without any errors. PWM can be adjusted in order to support almost all devices.
The best signals are created on a Raspberry Pi 3 Model B (or a faster board).



## Installation

### The easy way:

* `wget https://github.com/Appyx/gpio-reflect/releases/download/SOMEVERSION`

If there is a release for your Raspbian kernel you are lucky. If the kernel version does not match, you can just try a version that is similar enough.

To try the different releases you can just `insmod gpio-reflect.ko`.


### The manual way:
* Clone the repo and compile the module for the Raspberry Pi or another board ([really good tutorial](http://lostindetails.com/blog/post/Compiling-a-kernel-module-for-the-raspberry-pi-2)).
* `insmod` the module.

### The Raspbian (Stretch) way:
* `sudo apt-get update -y`
* `sudo apt-get upgrade -y`
* `sudo apt-get install raspberrypi-kernel-headers`
* `git clone https://github.com/Appyx/gpio-reflect.git`
* `cd gpio-reflect`
* `sudo make`
* `sudo insmod gpio-reflect [options]`

Check if it's working with: `dmesg |grep gpio`

### For a permanent installation (Raspbian only):

* `cd gpio-reflect`
* `sudo make install`
* add `gpio-reflect` to `/etc/modules`
* optional: create the file `/etc/modprobe.d/gpio-reflect.conf`
* optional: add the options to the file in the form: `options gpio-reflect param1=value1 param2=value2...`
* run `sudo depmod -a`
* `sudo reboot`
* run `lsmod |grep gpio` to see if it worked (you can also use `dmesg`)


## Usage

After insmodding the module a device appears under /dev/gpio-reflect.
This device can be used as a regular file.

Only one process at a time can access it, the others have to wait. But any action can be interrupted with ctrl+c.

* To record a signal just read the file with `cat` or something else.

* To send the recorded signal just wirte the signal with `echo` to the file.

You need `sudo` for interacting with the module. The scripts `irsend` and `irsniff` make the sudo access easier. Just execute the script with sude and add the signal as parameter.

You have to use the format of the recorded signal: 
`MILLIS.MICROS|MILLIS.MICROS|`...

So a signal with periods of `123.55|456.0|0.200` means: 123ms55us ON -> 456ms OFF -> 200us ON.
The last state is always pulled to zero after the period to protect the hardware.

## Options

You can configure the module for different use cases.
This is accomplished at `insmod`-time by appending `param=value`.

Possible parameters:

* `in`: Number for the input-pin (default=15).
* `out`: Number for the output-pin (default=18).
* `timeout`: Timespan in milliseconds after the last signal-component is recorded (default=1000).
* `max_size`: The maximum number of pulses a signal can contain before it is discarded (default=200).
* `min_size`: The minimum number of pulses a signal must contain in order to interpret it as signal (default=50).
* `freq`: The carrier-frequency (in kHz) of the signal, if pwm is used to transmit it (default=36).
* `pwm`: Integer value that acts as a divider. `0`=no pwm, `2`=50% duty cycle, `3`=33%, `4`=25%, and so on. Negative numbers invert the on/off periods. `1` can be used but `0` is better in terms of performance (default=2). 

You can check your inputs and possible recording failures with `dmesg -w`.


I use the module for receiving and transmitting infrared signals, but it can be used for any tpe of signal. But 433MHZ-RF for instance is not possible because of the heavy noise in the receiver.



