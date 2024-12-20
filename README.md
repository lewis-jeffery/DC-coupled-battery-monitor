## Summary
The myenergi "Zappi" can regulate an electric car charger so as to balance solar PV generation.  This avoids the purchase of expensive electrical power own solar is available.  The system works well.  

Addition of a house battery is completely possible but, depending on how it is wired, may preference charge/discharge of the car or the house. The Zappi can be configured to change the sequence if desired, provided that the power to/from that bettery is measured.   As supplied the Zappi can only measure AC coupled batteries (such as Tesla Powerwall) but many existing houshold batteries are DC coupled.

This project extracts the battery power reading from a typical DC coupled "hybrid inverter" using a Modbus connection.  This is a data connection only.  It does not interfere with battery or mains power wiring.   The battery power data is convertered into a mains frequency current proportional to the measured power and is applied to the Current Transformer terminals of the Zappi.  This emulates the CT that is present on an AC coupled battery and permits the in-built Zappi controls to operate effectively.

Note that coordination of house/car battery charge/discharge is the objective rather than accounting precision.  The number displayed on the myenergi app will be close enough for charger control purposes but do not be surprised if the energy balance is out by a few percent.

## Warnings
This is a prototype that has worked well for me, so far.   It may not be what you want.

Connection to the inverter via the Modbus interface (RS485) is required.  This is a low voltage connection but is likely to be near to other hazardous components.  Capacitors within the inverter can store sufficient energy to remain **lethal** even if the inverter is isolated from the PV cells and the grid.  Similary the Zappi has a low voltage connection for the CT but nearby components are hazardous.   You must have the necessary skills to identify and manage these hazards. 

I posted this concept in the [myenergi forum](https://myenergi.info/viewtopic.php?p=132908#p132908)   and received a few comments [(and this)](https://myenergi.info/viewtopic.php?p=133013#p133013).  If you are not in a position to analyse these comments and make an informed judgement of your own then this is not the project for you.

## Data from the inverter
A Modbus connection to the Sungrow SH5K-30 inverter is made using a Raspberry Pi 4B with a [RS485 HAT](https://www.waveshare.com/rs485-can-hat-b.htm) .  This particular HAT galvanically isolates the Raspberry from the RS485 signals.  Many cheaper, non-isolated versions are available.  I have observed significant common mode voltages on the data lines and consider isolation essential.  

Modbus details are readily available from vendors.  Sungrow data for a range of inverters is [here.](https://www.photovoltaikforum.com/core/attachment/235914-ti-20211231-communication-protocol-of-residential-hybrid-inverter-v1-0-23-en-pdf/)   Fronius, and others, use open standands as described [here.](https://www.fronius.com/en/solar-energy/installers-partners/technical-data/all-products/system-monitoring/open-interfaces/modbus-rtu)

The python script server.py is set to run on startup in /etc/rc.local on the Raspberry Pi.  Tools to monitor the server and to set a fixed output for calibration purposes are included in 'server tools.ipynb'.

At present the Raspberry Pi and HAT are powered by a USB-C plug pack.  A 48VDC (nominal) to 15V converter HAT is in the works so that the server can be powered from the house battery.  This is important if the car charger is wanted during islanding from the grid.

Data is transmitted via WiFi which is also needed at the car charger end and may also need to be available during a power outage.

## CT simulator
The CT simulator is based on an Arduino ESP32 Nano that polls the Raspberry server regularly for battery power.  This is multiplied by a sample of the grid voltage and scaled to represent the current that would be present in a CT on an equivalent AC coupled battery.  An analog signal is derived from the ESP32 PMW output which is filtered and fed to an opamp voltage to current converter.  

A sample of grid voltage is taken from the secondary of an isolation transformer and level shifted to suit the 0 - 3.3V capability of the ESP32.  A fixed offset voltage is required for this shift and is generated using a 5.6V zener.  A few more resistors than strictly required are included in the schematic.  If adjustment is necessary a few places for components on a PCB is useful.  A later version may replace these components with potentiometers.

A LTSpice simulation of this stage is included.

The schematic is included as CT_simulator_schematic.pdf and a rendering of the PCB is shown below.
![PCB](CT_simulator_PCB.png) 

Note the rendering of the PCB does not show the required isolation around the 240VAC terminals.  The prototype PCB was made on a desktop mill and isolation was included at the milling gcode stage.   Also a consequence of milling rather than use of a PCB fabrication shop is the absence of plated through holes which places some constraints on track routing.  A fab-shop version of this board is a potential next step.

Once the CT simulator is connected the Zappi the selected terminals should be declared as battery measurement in the Zappi configuration.  Battery power can then read on the myenegi app.  This version is scaled to for 3kW maximum house battery power charge and discharge.  Another scale may be better for your system.  The ADC input has 12 bit resolution over a 0 - 3.3V span and the PWM output is set at 5000Hz and 12 bit resolution.  Myenergi devices assume a CT ratio of 2500:1 so in this case  3kW of battery power is equivalent to 12.5A at 240VAC which would induce 5mA in the secondary of the equivalent CT.  This current is generated in the opamp voltage to current stage and delivered to the Zappi CT terminals.

Using the full ADC and PMW range effectively may require some balancing.  A convenient degree of freedom is the introducion of calibration_factor in the ESP32 code (line 32) which should be adjusted for best accuracy.  Note that the PMW filter has a phase shift of 22 deg at 50Hz which represents a PF of 0.92.   Compensation for this effect is lumped within calibration_factor.

A Jupyter Notebook with a tool to set a fixed output from the server is included to aid calibration.

