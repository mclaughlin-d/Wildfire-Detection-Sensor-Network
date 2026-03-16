## AFC0712DD Fan Control Demo
Contains a demo main.c and .ioc file which initialize pins PB_10 and PB_11 as
PWM signals, and turns the fans on and off.  

Important note: PWM duty cycle must be at least 30% to cold-start fans, according
to datasheet. 

### Configuration in .ioc
TIM2:  
- Prescaler: 39  
- Counter mode: Up  
- Counter period: 100  
- Channels 3 and 4 enabled, set to PWM generation

PB_10: CH3  
PB_11: CH4  

### Code
Starts PWM on both pins. Sets PB_10 to 50%, and turns off PB_11. Then, slowly 
turns PB_10 slower until off. Starts PB_11 and slowly turns to off as well.
