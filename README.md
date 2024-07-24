# demandflow
Reverse engineering of Itho Demandflow system

I got this idea because my Demandflow device is malfunctioning and a new one costs around 1200 Euro. Way to expensive. So I thought I make one by myself. This will 
take at least a year or so to come to a product. First step will be hardware control only by the ESP32 and the intelligence in Nodered. Later the intelligence could move 
also to the ESP. Also refer to the other files in this repo, showing a movie of a working valve and a photo of the valve itself. I have also attached the schematic of 
the circuit.

ESP32 to control a valve of the Itho Demanflow system. This  valve is a stepper motor made by Saia of type UCD2EZ7. It is an unipolair stepper
 motor with two coils and a middle branch. In the ESP there is no control software algoritm but only control of the hardware. Control is with Nodered
 by publising a JSON message to MQTT. See arduino sketch. The system is set up with 3 x 74HC575 circuit in order to control 6 valves with one serial 
 connection to rthe ESP32. This system will later duplicated in order to control 12 valves.