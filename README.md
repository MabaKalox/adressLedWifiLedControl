## Wifi Server

### HostName

* `http://led_strip.local`- mdns

### Endpoints

GET - `/`
Response: `200, Hello, world`

POST - `/set_led_strip`

* Request body: `multipart`.
    * `color_array` - list of hex encoded `W` `R` `G` `B`, e.g: `000f000f,000f0000`
* Response: `200, APPLIED`

## PINOUT

1. D2 – led strip data pin throught `50` ohm resistor.
2. D5 – wifi creditials reset button pin.
3. VIN – power pin thorough [1n4007](https://www.vishay.com/docs/88503/1n4001.pdf) diode.


