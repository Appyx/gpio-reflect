# gpio-reflect
LKM for sending and receiving infrared signals (and others)

## Installation
* Clone repo and compile the module on the Raspberry Pi or another board (testet only on RPI).
* `insmod` the module. (Sorry but i will not explain how to build a kernel module...)

## Usage

After inmodding the module a device appears under /dev/gpio-reflect.
This device can be used as a regular file.

Only one process at a time can access it, the others have to wait. But any action can be interrupted with ctrl+c.

* To record a signal just read the file with `cat` or something else.

* To send the recorded signal just wirte the signal with `echo` to the file.

You need `sudo` for interacting with the module.

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
* `max-size`: The maximum number of pulses a signal can contain before it is discarded (default=200).
* `min-size`: The minimum number of pulses a signal must contain in order to interpret it as signal (default=50).
* `freq`: The carrier-frequency (in kHz) of the signal, if pwm is used to transmit it (default=36).
* `pwm`: Integer value that acts as a divider. `0`=no pwm, `2`=50% duty cycle, `3`=33%, `4`=25%, and so on. Negative numbers invert the on/off periods. `1` can be used but `0` is better in terms of performance (default=2). 

You can check your inputs and possible recording failures with `dmesg -w`.


I use the module for receiving and transmitting infrared signals, but it can be used for any tpe of signal. But 433MHZ-RF for instance is not possible because of the heavy noise in the receiver.



