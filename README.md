#

![Plantcare][logo-plantcare-arduino]

Arduino code to handle Plantcare service.

## Methods glosary

* `#EnableWetSensor`: Enable wet sensor to read analog output.
* `#DisableWetSensor`: Disable wet sensor to read analog output.
* `#CloseWaterPump`: Close water pump to stop watering plantcare.
* `#OpenWaterPump`: Open water pump to wet plantcare.
* `#UpdateWetStatus`: Read each wet sensor value and post it to Plantcare server.
* `#ReadWetSensor`: Enable, read, and disable a wet sensor.
* `#PostWetStatus`: Post HTTP request with the wet value to Plantcare server.
* `#CheckWatering`: Check pending waterings from Plantcare and enable the involved water pumps.
* `#GetWaterPumpStatus`: Get water pump status from Plantcare server.
* `#GetHttpResponse`: Eval HTTP response.
* `#SendHttpRequest`: Send HTTP request via AT commands.

[logo-plantcare-arduino]: https://user-images.githubusercontent.com/6224703/99198674-723d5100-279a-11eb-8e41-0f32002cda4f.png "Plantcare"
