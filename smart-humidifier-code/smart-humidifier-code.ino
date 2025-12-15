/*
Host BLE non-stop
When connected to bluetooth, set:
  -RGB Led light(s) color
  -Screen on/off
  -Switch between autonomous / timed mode
  -

Display shows:
  -Temperature celsius
  -Humidity percentage
  -Humidity threshold
  -Selected mode
  -Animation

Autonomous mode logic:
    -Humidity percentage is read
    -Humidity below (threshold - hysteresis) (50% - 5% = 45%) => start humidifying
    -Humidifier runs for min 5 minutes
    -After min runtime:
        -If humidity is above (threshold + hysteresis) (50% + 5% = 55%) => stop humidifying
        -If humidity is between 45% and 55% => keep current state
    -If humidity rises above 55% before minimum runtime ends => ignore until minimum runtime is reached

Timed mode logic:
    -Interval is set through BLE
    -Humidification time is set through BLE
    -Humidify every X seconds for Y seconds

Bluetooth commands:
    -RRR;GGG;BBB (0-255) | "Minimum value is 0!" ; "Maximum value is 255!"
    -DON => Display On | "Display on"
    -DOFF => Display Off | "Display off"
    -AUTO => Autonomous mode | Autonomous mode
    -TIMED => Timed mode | Timed mode\nInterval:XXXX\nFor:XXXX\n
    -INTRVLXXXX => Timed mode humidification interval XXXX seconds (min 5 minutes; max 4 hours; must be more than FOR) | "Minimum is 5 minutes (300 seconds)" ; "Maximum is 4 hours (14400 seconds)" ; "Value can't be less than or equal to FOR (<for-value>)"
    -FORXXXX => Timed mode humidification for XXXX seconds (min 5 minutes; max 30 minutes; must be less than INTRVL) | "Minimum is 5 minutes (300 seconds)" ; "Maximum is 30 minutes (1800 seconds)" ; "Value can't be more than or equal to INTRVL (<intrvl-value>)"
    -None of these commands => "Unknown command"
*/

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    Serial.println("XD");
}