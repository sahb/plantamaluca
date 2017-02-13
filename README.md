# PlantaMaluca
An Arduino gardening system allowing remote monitoring via http and controlling a water pump powered by a PSU. Please note the current setup is made to be used with an Arduino Mega 2560.

## How it works
Once connected, it is possible to collect the parameters monitored (LDR and moisture values) or set moisture limits to turn on and off the water pump. The water pump will be activated or deactivated based on this limits.

The Arduino is set to be powered by the 5V standby power provided by a regular computer PSU. Before activating the water pump, the system will turn the PSU on, and then powers the water pump. This keeps the electricity usage at the lowest usage possible given this arrangement.

## Hardware
- Arduino Mega 2560 (other Arduinos can be used)
- ENC28J60 ethernet shield (other ethernet shields can be used)
- several wires
- 12V windshield washer water pump
- moisture sensor
- LDR sensor
- double relay shield

## Monitoring outputs
- JSON
- JSONP
- XML

## Usage
(Supposing the IP of the shield is 192.168.0.10)

### Passive monitoring
- JSON: http://192.168.0.10/?format=JSON
- JSONP: http://192.168.0.10/?format=JSONP&callback=callbackfunction (replace 'callbackfunction' by the function of your choice)
- XML: http://192.168.0.10/?format=XML

### Changing watering limits
- Changing superior limit (flooding - turn pump off) - http://192.168.0.10/?action=newsup&value=1000
- Changing inferior limit (dry - turn pump on) - http://192.168.0.10/?action=newinf&value=500

On both cases, the result will be the updated values displayed in JSON format
