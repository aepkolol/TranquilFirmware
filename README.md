# TranquilFirmware

[![MIT License](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/)
[![CodeFactor](https://www.codefactor.io/repository/github/acvigue/tranquilfirmware/badge)](https://www.codefactor.io/repository/github/acvigue/tranquilfirmware)

Fork of firmware for the ESP32-based Tranquil kinetic sand drawing robots

This is modified from acvigue's version, using slightly different hardware and modified printed parts. I'll attempt to do a more in-depth build tutorial in this repo.

## Requirements

This is *not* a drop-in replacement for Rob Dobson's original RBotFirmware! Many things have been changed/moved around.

Hardware:
- A 3D printer with a 350mm+ bed size
- A bunch of filament
- Hot glue gun
- super glue
- Microswitch (limit switch) - 
- ESP32 (I used an esspressif ESP32 dev kit from Amazon. Be cautious of using the same pins if following me exactly)
- 2x NEMA17 "Pancake" stepper motors (20mm wide) - https://www.amazon.ca/dp/B06ZY5KDSG
- A single modified hall effect sensor - https://www.amazon.ca/dp/B07VDJSP3D
- A single 3 inch "Lazy suzan" bearing (In place of a 4 inch bearing. More on this later) - https://www.amazon.ca/dp/B0BXN9TQ95
- 2x 10mm*3mm Magnets - https://www.amazon.ca/dp/B0B5ZVJN73
- 400mm GT2 belt (This was listed on the build page for the origional version of this. This doesn't fit that, but works here) - https://www.amazon.ca/dp/B09HK2H6S8
- Steel ball (No size was listed. I purchased a kit of "stainless steel" bearings to play with different sizes. Keep in mind, real stainless steel isn't magnetic. I took a gamble that the seller lied about its composition and was correct, and the ones linked ARE magnetic) - https://www.amazon.ca/dp/B09C1J2JPS
- DC Barrel jack - https://www.amazon.ca/dp/B01M0RFV34
- 12v DC power supply - No link, I have a bunch laying around. Aim for ~3A without LEDs. More depending on how many LEDs you plan on using.
- Arduino capable SD card reader - https://www.amazon.ca/dp/B07V78MD81
- Buck converter (Only need a small buck if you're not using LEDs. Match your current draw for 5v if using 5v LEDs) - https://www.amazon.ca/Converter-Voltage-Regulator-Voltmeter-Adjustable/dp/B07XXSK68F
- M3 heatset inserts - https://www.amazon.ca/iplusmile-Embedment-Threaded-Printing-Projects/dp/B087N4LVD1
- A bunch of countersunk M3 screws - https://www.amazon.ca/100pcs-Screws-Machine-Countersunk-Stainless/dp/B09DC4K6V7

> This project requires an operational [TranquilAPI](https://github.com/acvigue/TranquilAPI) server! Run your own on CloudFlare Workers!

## Notes

The firmware is setup to automatically check for updates from a central server. The firmware binaries are built upon commit to this repository and are automatically uploaded to the OTA server.

See [cloudflare-ota-server](https://github.com/acvigue/cloudflare-ota-server) for more information. 

## Robot Configuration Reference

Robot configuration is stored in NVRAM and can be viewed by sending GET request to `/settings/robot` and can be changed by POSTing JSON to `/settings/robot`

Similarly, wifi/ntp/scheduler/light settings can be changed at

- /settings/wifi < Network settings (supports WPA2-Enterprise)
- /settings/ntp < time zone and ntp server
- /settings/scheduler < stores scheduled commands
- /settings/lights < LED lights
- /settings/security < used to store PIN code
- /settings/tranquil < only used for storing TranquilAPI encrypted JWT

> LED strip _must_ be attached to GPIO5

```js
//See RobotConfig.example.json for an uncommented version.
{
  "robotConfig": {
    "robotType": "TranquilSmall", //custom robot config name, please change if building your own and submitting PR.
    "cmdsAtStart": "", //commands to run on startup, seperated by ';' ex. "G28" to home.
    "evaluators": {
      "thrContinue": 0, //must be 0
      "thrThetaMirrored": 1, //to mirror theta axis or not (flip drawings)
      "thrThetaOffsetAngle": 0.5 //rotate drawings around the bed (DEGREES)
    },
    "robotGeom": {
      "model": "SandBotRotary", //keep SandBotRotary
      "motionController": {
        "chip": "TMC2209", //tmc2208 or tmc2209, omit whole object if not using Trinamics, if you are, rest of params are required.
        "TX1": 32, //UART for drv1
        "TX2": 33, //UART for drv2, rest of params are self explanatory.
        "driver_TOFF": 4, //hysterisis TOFF time
        "run_current": 600, //motor run current (mA)
        "microsteps": 16, //motor microsteps
        "stealthChop": 1 //stealthchop, sets stealthchop2 for 2209, stealthchop1 for 2208,2130
      },
      "homing": {
        //homing string, axis A is rotary, B linear.
        "homingSeq": "FR3;A+38400n;B+3200;#;A+38400N;B+3200;#;A+200;#B+400;#;B+30000n;#;B-30000N;#;B-340;#;A=h;B=h;$",
        "maxHomingSecs": 120
      },
      "blockDistanceMM": 1, //movement resolution in mm (keep at 1, lower stalls bot)
      "allowOutOfBounds": 0, //keep 0
      "stepEnablePin": "25", //motor enable GPIO pin
      "stepEnLev": 0, //motor active logic level
      "stepDisableSecs": 30, //seconds after last move to turn motors off
      "axis0": {
        "maxSpeed": 15, //no idea
        "maxAcc": 25, //no idea
        "maxRPM": 4, //max RPM for rotary axis
        "stepsPerRot": 38400, //steps (including microsteps) for one full rotation of the primary rotary axis
        "stepPin": "19", //step pin for this axis
        "dirnPin": "21", //dir pin for this axis
        "dirnRev": "1", //is direction reversed?
        "endStop0": {
            "sensePin": "22", //endstop GPIO pin
            "actLvl": 0, //triggered at what level?
            "inputType": "INPUT"
        }
      },
      "axis1": { //same as above, changes highlighted
        "maxSpeed": 15,
        "maxAcc": 25,
        "maxRPM": 30,
        "stepsPerRot": 3200,
        "unitsPerRot": 40.5, //for each rotation of the upper central gear, how much does the linear arm move in MM
        "maxVal": 145, //maximum MM the linear arm can go out from center
        "stepPin": "27",
        "dirnRev": "1",
        "dirnPin": "3",
        "endStop0": { "sensePin": "23", "actLvl": 0, "inputType": "INPUT" }
      }
    },
    "fileManager": {
      "spiffsEnabled": 1, //must be 1
      "spiffsFormatIfCorrupt": 1, //self explanatory, i advise against it.
      "sdEnabled": 1, //must be 1
      "sdSPI": 0, //OPTIONAL, set to 1 for sdspi, if omitted, SDMMC is used.
      "sdMISO": 0, //REQUIRED FOR SDSPI, omit for SDMMC, miso pin
      "sdMOSI": 0, //REQUIRED FOR SDSPI, omit for SDMMC, mosi pin
      "sdCLK": 0, //REQUIRED FOR SDSPI, omit for SDMMC, clock pin
      "sdCS": 0, //REQUIRED FOR SDSPI, omit for SDMMC, CS pin
      "sdLanes": 1 //1 or 4, sets bus width for SDMMC, not used for SDSPI
    },
    "ledStrip": {
      "ledRGBW": 1, //1 for SK6812, 0 for ws2812
      "ledCount": "143", //led count
      "tslEnabled": "1", //does robot have TSL2561
      "tslSDA": "16", //if so, i2c pins.
      "tslSCL": "17" // ^^^
    }
  },
  "cmdSched": { "jobs": [] }, //don't edit, doesn't do anything but fw still relies on it being here
  "name": "Tranquil" //robot name, not hostname, that is set in wifi settings.
}
```
-----

Here is where my tutorial starts. I've never wrote one of these before, so raise an issue if you need any questions answered. I'll add in more photos if needed, or once I get around to it.

THE BUILD.

A quick note on printed parts: 
Firstly, print off all the required pieces. I used a fancy PLA for most of the larger enclosure parts and PETG for the rest. I had some issues getting the PLA to stick to the bed nicely, even with a well tuned Voron 2.4. I can imagine it will be way harder without proper leveling and bed mesh calibrations. This isn't a print quality quide, but to solve my issues I did 2 initial layers in PETG, then swapped it out for PLA for the rest. 

The inner workings:

This is the first part I had to modify. Firstly, the 400mm GT2 belts I ordered didn't fit around the supplied theta gear, and second, I ordered a 3" lazy suzan bearing on accident. It turned out to be good luck, because I needed the smaller bearing to fit the clearances I needed to make the belt fit. To start off with, use a soldering iron to push in the M3 heatset insets into the four holes on this part. Make it flush with the top of the gear. It might go below the part where it sits on the bearing, this is fine. 

<img width="677" alt="Screenshot 2023-08-19 135231" src="https://github.com/aepkolol/TranquilFirmware/assets/28993017/7815097a-e67c-481c-81e8-24714000c2f8">

Now, flip it upside down and press fit the 10mm*3mm magnet. You can superglue it, but I would wait until you test the hall effect sensor. You may need to flip the magnet upside down to trigger the sensor. Put this part off to the size for now. 

Take the mounting plate part now, and attach the lazy susan bearing. I printed this part before I realized I ordered the wrong bearing, so I just made a quick tool to center the bearing and drilled holes through. I'll edit the part with the holes already made, but if I haven't by the time you read this, drilling the holes works. I just used an M3 screw and a nut on the other side. Also attach the stepper motors now. Leave the off-centered one loose for now, you'll need to adjust this soon to tighten the belt. Attach a GT2 pully onto the offcentered stepper motor. (Not pictured, imagine its on there)

<img width="733" alt="Screenshot 2023-08-19 140241" src="https://github.com/aepkolol/TranquilFirmware/assets/28993017/24e99153-b3dd-4491-a143-df582057a140">

Now put the theta gear onto the mounting plate. I found it easiest to hold the entire assembly upside down with the belt around the gear, then lineup the lazy suzan bearing to fit the gear. Once its lined up, flip it back the right way and wrap the belt around the pully, making sure it doesn't slip off the gear. Then pull the stepper out to tighten the belt, and tighten the screws down. (Belt is also not pictured. Modeling belts is harder than its worth). While you're at it, press on the printed center gear. Don't push it down too far, its a pain to pull it back up to align it later.

<img width="706" alt="Screenshot 2023-08-19 141012" src="https://github.com/aepkolol/TranquilFirmware/assets/28993017/b240028c-01ee-462f-9b42-12760fa51ae2">

Now we can attach the center arm thinger. I also modified this part. I found the teeth sometimes wouldn't mesh because the rack gear could bow outwards sometimes. So I made it solid on top. I also made a hole to attach a magnet holder, and the magnet holder itself. Super glue a magnet into the holder, then glue the holder to the arm. The next steps are pretty obvious. Just screw mesh the rack to the pinon and screw down the attachment part. 

<img width="781" alt="Screenshot 2023-08-19 141431" src="https://github.com/aepkolol/TranquilFirmware/assets/28993017/6b6d7c7b-70f8-4854-92bd-ba77d36e30c4">

Now its time to take a break from mechanical assembly, and move onto electrical for a bit. 

There are going to be many ways to wire this up. How you do it also depends on the exact hardware you have. I'll show you what I did for mine, but it isn't very graceful. I'd recomend buying that nice PCB Acvigue made, but do what you can/want to do. 

I made a mounting plate to fit my buck converter onto the posts on the bottom of the theta mounting plate. This is where the power from the barel jack will enter and power everything. I'd recomend first powering it from your wall plug and adjusting the pot to 5-5.1v right now. That way you don't fry everything with an unknown voltage when you first power it on. If you ordered the ones I linked, you can just use the digital LED display, otherwise you need a multi-meter to check the output voltage.  

<img width="719" alt="Screenshot 2023-08-19 142001" src="https://github.com/aepkolol/TranquilFirmware/assets/28993017/63795552-15c9-4aed-b8de-29ba6c56dba0">

I then hot glued all the PCBs to the bottom of the mounting plate. I told you it wasn't very graceful, but its not like anyone will see. Just make sure not to short anything out. 

I'll start with the hall effect sensor. I hate them. Some are adjustable, with both location and sensitivity. These ones aren't. So, I desoldered the actual hall effect sensor from the PCB and added some wires to extend the range and give me some movement to adjust its postion and pickup the magnet. Be careful not to short out the pins, they are spaced pretty close to one another. Ideally you should use some heatshrink on the pins. I rolled the dice. 

*Picture of shitty mod*

Luckily, the hole is a good size to feet this tiny sensor and the wires through. Also luckily, this sensor has two LEDs on it. One that says its powered, another when its triggered. Remember when I said to not superglue the magnet yet? This is why. Power on the sensor, either from 5v, or off the ESP32. Don't worry about the sensor wire yet. Just power it on, and play with it for a minute. Take the magnet and pass it close to the sensor and watch for the second green LED to light up. Did it? Good. You didn't break anything and it still works. Now pay attention to how the magnet is positioned when it triggers the sensor. Which polarity trips it. You want to feed the sensor and wires into the hole and spin the gear around and see if it trips. When its in a position that trips it reliably, you want to add some hotglue into the hole/on the sensor to keep that position. Now you can finish wiring it to the ESP32. Attach VCC to 3.3 or 5v. I used 5v, its usually more reliable. I then hooked the digitial output to pin 35 on the ESP32. I'll make a pinout list later on. 

Now you may as well hookup the stepper motor drivers and the stepper motors. I used BTT 2208 stepper drivers. I had some spare already from upgrading the drivers to my 3D printer. 2209 also works, the pinout may be different. First off, there are two power souces. One for the motor itself, and one for the logic. I soldered wires from VM and GND to the INPUT (12v) side of the buck converter on both drivers. You should have seperate wires going from each driver to the buck and not daisy chain them. It might be fine, but good practice on higher load wires. I then attached VIO and the second GND to the OUTPUT (5v) side of the buck. Don't get these mixed up, it will cause a short and burn something. Now you need 4 wires ran from each driver to the ESP32. DIR, STEP, TX and EN. 

I did:

Stepper 1:
Step: 27
Dir: 3
TX1: 33
EN: 25

Stepper 2:
Step: 4
Dir: 21
TX2: 32
EN: I ran a wire to the EN pin on driver1. You can also just attach a wire to 25 as well. This just enables the steppers, so they share the same pad. 

Now you can solder the stepper wires to the drivers. The wire coloring isn't universal, so you will need to match it according to the spec sheet of your motors. I did it like this..

A+ - OA1
A- - OA2
B+ - OB1
B- - OB2

A good rule of thumb for steppers is.. If it makes a weird sound and twitches, swap any two wires. If its running backwards, you can change that in the config. 

Now wrap the pins in electrical tape, heat shrink the whole thing. Just make sure it wont short off anything and tuck them away. 

![image](https://github.com/aepkolol/TranquilFirmware/assets/28993017/adf7719a-62a5-489b-84c5-fbc423e7fdcd)

For the second limit switch, I didn't use a hall effect sensor. I was going to, but I couldn't get it positioned in a way to trigger it properly. I hate them. So I used a micro limit switch. I estimated two longer strands of wire and soldered it to the C (common) and NO (Normally open) pins on the microswitch and used some small self taping screws to hold it in its mount. Eventually you will super glue this in position, but for now you can leave it dangling. I hooked one pin to GND on the ESP32 and the other to GPIO34 on the ESP32. 

The SD card reader. 

Another part I hot glued to the plate. Like every other PCB, I desoldered the header pins so I could hotglue the PCB flat. 

VCC - ESP32 3.3v
GND - ESP32 GND
MOSI - 23
MISO - 19
CLK - 18
CS - 5

acvigue mentions LEDs data pin MUST connect to GPIO5 on the ESP32, however CS is GPIO5 on mine. So I don't know. I haven't got LEDs for it yet, so I'll cross that bridge when I get to it. 

Now lets do some software stuff. 

Firstly, you need visual studio code and the platformIO extension. If you don't get it. Google how. I'm going to assume you have used platformIO and visual studio before. Google can fill in some other gaps. 

Clone the firmware and webapp repos to your machine. 

https://github.com/acvigue/TranquilFirmware
&
https://github.com/acvigue/TranquilVue

You will need NodeJS and yarn installed. Maybe other things, I just already have a ton of crap installed on my PC. Do that, then open a terminal in the TranquilVue folder. Run 

```yarn```

and then 

```yarn build```

If it doesn't show any errors and builds the website to a new "dist" directory, congrats. You built it. Otherwise, do typical programing stuff to figure out why it failed. Probably don't have something installed. 

Now go back to the firmware folder and make a new folder called "data" in the root of the folder (with the other folders called "src" and "lib"). Copy the contents of the "dist" folder from the previous step into this data folder. The contents, not the folder itself. 

Also in the firmware folder, there is a platformio.ini file. Change the "upload_port" and "monitor_port" to match the COM port of your ESP32 and save it. Now in the platformIO extension tab, click on "erase flash" to wipe it clean. Then click "build filesystem immage" and when that is complete hit "upload filesystem image". That will take a minute, the you can click "upload and monitor". If all goes well, it will build the firmware and flash your ESP32. 

It should boot up, and in the monitor tab it will show it created an access point. Connect to this with your phone or something then either wait for a "login" browswer window to auto open, or go to 8.8.4.4 in a browswer. A wifi settings window should open on that webpage, setup your wifi in there and allow it to reboot. Now watch that monitor terminal again. It will show you what IP address the ESP32 got from the router. Back on your wifi, connect to this IP to load the frontend of the machine. 

If at any step of trying to connect to the webpage your browser gives an error, this happened to me too. When you compiled the website to drag onto the ESP32, it compresses the code for the webite side of things. None of my browswers liked this. In the root of the vue folder there is a file called "vite.config.ts". 

Where it says 

``` plugins: [vue(), svgLoader(), viteCompression({ ext: "br", algorithm: "brotliCompress", deleteOriginFile: true, threshold: 10})],```

change it to say 

``` plugins: [vue(), svgLoader()],```

You might be over the 4mb filesystem size now. I just deleted one of the icon.png images to save the needed space. Now it should load. 

Now you need to send the config over to it, so it will work with your setup. To do this, I used an app called "postman" to make the POST/GET requests to the ESP32. 

(I have to leave now, this will TBD)
