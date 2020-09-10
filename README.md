# Fed-NSF-Efficient-Field-Testing
All of the files in the project will need to be edited to include information for api keys and ThingSpeak channel ID.

The only changes that need to be made to the Temperature_Control_Panel are changeing the ****** to the ThingSpeak channel ID.

There are multiple changes that need to be made to the Arduino_Field_Testing file. All of the changes that need to be made are before the setup.
- **ThingSpeakChannelIDTemp** Temperature ThingSpeak Channel ID
- **ThingSpeakChannelIDHigh** HighTemperature ThingSpeak Channel ID
- **ThingSpeakWriteKeyTemp** Temperature Channel Write API Key
- **ThingSpeakReadKeyHigh** HighTemperature Read API Key

- **IFTTT_Key** IFTTT API Key


There are also three .m files. These files are already running on ThingSpeak so there is no need to update them unless a new API key is generated for a ThingSpeak channel, Open Weather Map, or IFTTT. The averageTempDiff and checkWeather are meant to run once a day. The weatherData is meant to run every ten minutes, it looks better if it runs exactly on the ten minute mark (0000, 0010, 0020, etc.) but it should work no matter the start time.
