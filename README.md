# Morse Tetris: Tetris with controls sent via morse code!

This is the repository for our 2025 BitHacks Hackathon at UCI!


There are two .ino files designed to run on 2x ESP32 S3 development boards, one is the controller, and one displays the game.
While the original plan was to have the game microcontroller read from a mic to listen for the morse code, we were unable to get the mic to transmit any data so we rely on a Bluetooth protocol for communication.

The morse code aspect is more of a funny transmission mode, and the controller would have sent the correct morse codes to the speaker when you press a button - it wouldn't rely on the user to manually type morse code.
