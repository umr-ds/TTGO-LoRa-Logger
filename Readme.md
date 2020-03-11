# TTGO LoRa Logger

A simple tool to log LoRa communication for further evaluation designed for the TTGO T-Beam.

## Functionality

- Continuously validate GPS (set position and date/time)
- Listen for LoRa messages (static configuration, see `src/main.cpp:153`)
- Save messages and keepalives to internal SPIFFS file system
- Serve internal filesystem via HTTP

## Using the logger

### WiFi Connection

A WiFi can be configured in the code, where the Logger will try to connect to during bootup. If non successful, it will continue operation on AP mode:

```
SSID: TTGO T-Beam LoRa Logger
Password: supersicher
IP: 192.168.4.1
```

### Accessing Logfiles

The logfiles are named `recv_xxxx.csv`, and can be obtained via `HTTP GET`, e.g. [http://192.168.4.1/recv_0000.csv](http://192.168.4.1/recv_0000.csv).

> Note: A request to the root file is always forwarded to the current logfile written to, hence [http://192.168.4.1/](http://192.168.4.1/) will retrieve the most recent logfile. 

The logfile contains the following fields:

- `ts`:
- `gps_sat`:
- `gps_age`:
- `lat`:
- `lon`:
- `alt`:
- `cnt`:
- `len`:
- `rssi`:
- `snr`:
- `freq_err`:
- `msg`:

### Resetting Storage

Press and hold the user button 3s during bootup. A message on the serial console will show up, and inform, that the internal storage has been deleted.

## Caveats

The internals storage is limited to 4 MB, which is easily filed with messages, since a lot of metadata is transmitted. Therefore it is only useful for shorter experiments and should be formatted afterwards. 

