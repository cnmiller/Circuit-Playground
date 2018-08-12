# Circuit-Playground
Adafruit Circuit Playground Projects

##Midi Converter
How to converter MIDIs so they are playable on the Adafruit Circuit Playground

1. Use a Midi-to-Arduino converter such as [https://extramaster.net/tools/midiToArduino/](https://extramaster.net/tools/midiToArduino/) to get a raw tones file (select Raw device option on https://extramaster.net/tools/midiToArduino/)
2. Save the raw tones output to a file named rawoutput.txt
3. Use the bash one-liner `cat rawoutput.txt | sed '/D/d' | cut -d" " -f2,3 | cut -d, -f 1,2 | sed -e 's/^/CircuitPlayground.playTone(/' | sed 's/$/);/'` to convert the newly-created rawoutput.txt to Adafruit Circuit Playground library compatible tone list.
4. Insert the Adafruit Circuit Playground library compatible tone list into your project where appropriate.
5. Enjoy!

