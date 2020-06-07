# VK2IDL_CW_Decoder
A Morse Code Decoder based on WB7FHC's Simple Morse Code Decoder.

The CW decoder uses the same basic LM567 circuit designed by Budd Churchward WB7FHC which is connected to an Arduino Nano. I have made some software changes to suit my circumstances. These include a 'live' adjustable farnsworth setting, a much faster sweep tuning function by reducing the tuning range, and changes to the general operation and LCD layout. Ive also removed the sidetone and speaker connection as I prefer a softer sine-wave side tone which I can generate externally.

When running the original WB7FHC version it used to get itself into a mode where it would no longer decode morse. This was most apparent when there had been no morse input for a while. The only way to fix it was to press the reset button. Budd also indicated the reset should be used occasionally. I found that the values of the 'startUpTime' and 'startDownTime' variables became very large whenever there was a delay in receiving any morse input. To correct this I simply reset these values whenever the SWEEP button is pressed. So if you are in a situation where after a while you cannot seem to correctly decode morse, simply press the sweep button which will retune the decoder input frequency to the current tone value AND reset the timers and default values.

INSTRUCTIONS FOR USE OF THE MORSE DECODER

Press the FILTER (Left) switch to adjust the noise filter value. 
The filter introduces a delay when reading the morse input in order to reduce the effect of noise pulses on the input signal.The filter    value is displayed on the LCD as Fl=x where x is the filter value from 0 to 8.  The larger the value the greater the delay. Increase the value on noisy signals to improve readability.

Press the SWEEP (Right) switch to tune the digital pot attached to the LM567 tone decoder. 
The function will sweep up and down until a lock is made on the incoming tone frequency. The filter value is displayed on the LCD as T=xxx where xxx is the filter value. The default value of 201 represents a tone frequency of around 400Hz.

Press the Fansworth switch to set the Farnsworth value. Farnsworth settings are used for morse training by sending morse characters at a higher speed (say 10 wpm) while the character spacing is around 5 or 8 wpm. This allows a student to learn to recognise the pattern of the incoming characters at a more relistic speedspeed while still allowing enough time to think between characters. The farnsworth setting is  displayed as Fx where x is the selection for the spacing between characters. Values are: FO (Off), F5 (5 wpm spacing) or F8 (8 wpm spacing). 
   
The WPM=xx value on the LCD is the calculated wpm speed of the incoming morse.

The audio input can be fed directly from any suitable audio output (speaker, aux or headphone socket) on your radio. Alternatively you could connect a microphone with a preamplifier to the input to 'hear' morse from virtually any nearby source. 
