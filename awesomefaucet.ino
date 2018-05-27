/*
 Demonstrates the use of an Ultra Sonic Sensor.
 Displays result to computer screen via USB cable
HC-SR04 Distance Sensor Hookup:
    HC-SR04 Ping distance sensor Hook-up
    HC-SR04 Gnd to Arduino Gnd
    HC-SR04 Echo to Arduino pin 3
    HC-SR04 Trig to Arduino pin 2
    HC-SR04 Vcc to Arduino 5V
*/

// Define HC-SR04, LED and water digital I/O pins
#define TRIG                    2
#define ECHO                    3
#define LED                     5
#define WATER                   A1

// Constants
#define DEBOUNCE_TIME_MS        400                                                                 // Keep the water on for this long minimum.
#define UPDATE_RATE_MS          20                                                                  // Update rate.
#define OBJECT_HEIGHT_THRESHOLD 0.2 * 25.4                                                         // Distance above floor object must be to be detected.
#define LEAKAGE_RATE            0.5E-3                                                              // Number of mm subtracted from max every UPDATE_RATE_MS.
#define MIN_LEAKAGE             0.005                                                               // Minimuma allowed leakage (short foot)
#define MAX_LEAKAGE             0.035                                                               // Maximum allowed leakage (tall foot)
#define DATA_IIR_RATE           0.65                                                                // Data for detection IIR filter constant, 0 means no filtering, 0.9999 means very long.
#define MAX_IIR_RATE            0.001                                                               // Data Ifor maximum IIR filter constant, 0 means no filtering, 0.9999 means very long.
#define MAX_FLOOR               1 * 25.4                                                            // Don't let the maximum go below this level, helps avoid stuck states.
#define MAX_ALLOWED             10 * 25.4                                                           // Do accept any new readings above this level as presumably nonsense.
#define WATCHDOG_SCALE          60 / UPDATE_RATE_MS * 1000 / 3.33                                   // Watchdog time scaler to minutes. Loop seems to be about 3.33x slow.
#define WATCHDOG_TIME           3 * WATCHDOG_SCALE                                                  // Max allowed water time in minute increments before assert.
#define FOOT_CAPTURE_TIME       10                                                                  // Number of cycles from water on to foot height determination

// Variables
bool    water_on                = 0;                                                                // Water state variable.
bool    calibrating             = 1;                                                                // Initial startup state to indicate calibrating.
bool    led_on                  = 0;                                                                // Led state variable. Only used during initial calibration phase.
double  distance                = 0;                                                                // IIR filtered distance.
double  max_distance            = 0;                                                                // Keep track of max value ever seen.
int     debounce_timer          = 0;                                                                // Once water is on keep it on for DEBOUNCE_TIME.
long    watchdog_timer          = 0;                                                                // If water on for too long assert.
bool    foot_capture            = 0;                                                                // Flag that indicated that foot height has been determined
double  leakage                 = 0.01;                                                             // Amount subtracted every cycle. Load for the maximum value.
double  max_at_water_on         = 0;                                                                // Used to determine leakge based on foot height
int     foot_capture_timer      = 0;                                                                // Soft timer used to delay foot capture
double  distance_for_max        = 0;                                                                // Super filtered version of reading for max determination
double  reading                 = 0;                                                                // Lightly filtered version of reading used for foot detection

// Program setup
void setup() {
    Serial.begin(9600);                                                                             // initialize serial communication at 9600 bits per second to display results.
    pinMode(TRIG,         OUTPUT);                                                                  // Setup the trig pin as an output.
    pinMode(ECHO,         INPUT);                                                                   // Setup the echo pin as an input.
    pinMode(LED,          OUTPUT);                                                                  // LED pin for showing tripped distance.
    pinMode(WATER,        OUTPUT);                                                                  // Positive output pin, high when tripped.
    digitalWrite(WATER,   LOW);                                                                     // Start low.
}
// Get Sensor Reading
double sensor_distance() {
    double distance = 0;                                                                            // Currently measured distance.
    digitalWrite(TRIG, LOW);                                                                        // Set TRIG pin low.
    delayMicroseconds(2);                                                                           // Wait 2us.
    digitalWrite(TRIG, HIGH);                                                                       // Set TRIG pin high.
    delayMicroseconds(10);                                                                          // Wait 10us.
    digitalWrite(TRIG, LOW);                                                                        // Set TRIG pin low - starts transmit.
    distance = (pulseIn(ECHO, HIGH)/2)/2.91;                                                        // Calculate actual distance (in mm) based on echo time.
    return distance;
}
// Print status to com port for debug
void print_status() {
    Serial.print("Distance = ");                                                                    // 
    Serial.print(distance);                                                                         // 
    Serial.print(" mm,   ");                                                                        //
    Serial.print("Max = ");                                                                         // 
    Serial.print(max_distance);                                                                     //
    Serial.print(" mm,   ");                                                                        //
    Serial.print("Water = ");                                                                       // 
    Serial.print(water_on);                                                                         //
    Serial.print(" ,   ");                                                                          //
    Serial.print("timer = ");                                                                       //
    Serial.print(debounce_timer);                                                                   //
    Serial.print(" ,   ");                                                                          //
    Serial.print("leakagex100 = ");                                                                 //
    Serial.println(leakage * 100);                                                                  //
}

void assert() {
    Serial.println("\nWatchdog timer assert. Water disabled. Reset power to try again.");        
    while (1){
        digitalWrite(WATER, 0);                                                                     // Set water Off.
        digitalWrite(LED, 1);                                                                       // Toggle LED.
        delay(35);                                                                                  // 35ms
        digitalWrite(LED, 0);                                                                       // Toggle LED.
        delay(35);                                                                                  // 35ms
    }
}
// Main
void loop() {
    delay(UPDATE_RATE_MS);                                                                          // Wait UPDATE_RATE_MS second before taking next reading.
    reading = sensor_distance();                                                                    // Go get sensor reading
    if (reading <= MAX_ALLOWED) {                                                                   // Throw out crazy readings
        distance = (1 - DATA_IIR_RATE) * distance + DATA_IIR_RATE * reading;                        // Data IIR Filter.
        distance_for_max = (1 - MAX_IIR_RATE) * distance_for_max + MAX_IIR_RATE * reading;          // MAX IIR Filter.
        }
    
    calibrating = (max_distance < 0.95 * distance) && calibrating;                                   // Check for initial calibration
    if (max_distance <= MAX_FLOOR) max_distance = MAX_FLOOR;                                         // Apply MAX_FLOOR.
    if (distance_for_max >= max_distance && distance_for_max < MAX_ALLOWED)                          //
            max_distance = distance_for_max;                                                         // Increase max_distance.                                                               //
    if (distance <= max_distance - OBJECT_HEIGHT_THRESHOLD) {                                        // Detect object presence.
        debounce_timer = DEBOUNCE_TIME_MS / UPDATE_RATE_MS;                                          // Start de-bounce timer.
        water_on = 1;                                                                                // Set water state variable.
        max_at_water_on = max_distance;
        if (!(foot_capture_timer == FOOT_CAPTURE_TIME)){
           foot_capture_timer++;
           leakage = MIN_LEAKAGE;
           foot_capture = 0;
          }
        else if (!foot_capture){
            leakage = LEAKAGE_RATE * (max_at_water_on - distance);                                   // max_dsiatance - distance = foot_height
            if (leakage < MIN_LEAKAGE) leakage = MIN_LEAKAGE;
            if (leakage > MAX_LEAKAGE) leakage = MAX_LEAKAGE;
            foot_capture = 1;
        }
    }
    else {
        debounce_timer -= 1;                                                                         //       
        if (debounce_timer <= 0){                                                                    //
            water_on = 0;                                                                            //
            debounce_timer = 0;                                                                      //
            foot_capture = 0;
            foot_capture_timer = 0;
        }
    }
    if (calibrating){                                                                                // Blink LED during calibration.
        if (led_on){                                                                                 //
            digitalWrite(LED, LOW);                                                                  //
            led_on = 0;                                                                              //
        }                                                                                            //
        else{                                                                                        //
            digitalWrite(LED, HIGH);                                                                 //
            led_on = 1;                                                                              //
        }                                                                                            //
      }                                                                                              //
    else
        digitalWrite(LED, !water_on);                                                                // Set LED state.
    digitalWrite(WATER, water_on);                                                                   // Set water state.
    print_status();                                                                                  // Print status to serial port.
    if (water_on)                                                                                    // Check water status for watchdog.
        watchdog_timer++;                                                                            // increment watchdog timer if on.
    else {                                                                                           //
        watchdog_timer = 0;                                                                          // Clear timer if off.
        leakage = MIN_LEAKAGE;                                                                       //
    }                                                                                                //
    if (watchdog_timer >= WATCHDOG_TIME)                                                             // Check the timer.
        assert();                                                                                    // Assert if timer reached.
    max_distance -= leakage;                                                                         // Leaky integration back down.
}






