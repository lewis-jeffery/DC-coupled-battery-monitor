## Summary
The myenergi "Zappi" can regulate an electric car charger so as to balance solar PV generation.  This avoids the purchase of expensive electrical power own solar is available.  The system works well.  

Addition of a house battery is completly possible but, depending on how it is wired, may preference charge/discharge of the car or the house. The Zappi can be configured to change the sequence if desired, provided that the power to/from that bettery is measured.   As supplied the Zappi can only measure AC coupled batteries (such as Tesla Powerwall) but many existing houshold batteries are DC coupled.

This project extracts the battery power from a typical DC coupled "hybrid inverter" using a Modbus connection.  This is a data connection only.  It does not interfere with battery or mains power wiring.   The battery power data is convertered into a mains frequency current proportional to measured power that is applied to the Current Transformer terminals of the Zappi.  This emulates the CT that is present on an AC coupled battery and permits the in built Zappi controls to operate effectively.

## Disclaimer
This is a prototype that has worked well for me, so far.   It may not be what you want.

I posted this concept in the [myenergi forum](https://myenergi.info/viewtopic.php?p=132908#p132908)  [(and this)](https://myenergi.info/viewtopic.php?p=133013#p133013) and received a few comments.  If you are not in a position to analyse these comments and make an informed judgement of your own then this is not the project for you.
