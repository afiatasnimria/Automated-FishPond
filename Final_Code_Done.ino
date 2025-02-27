#include <SimpleTimer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Servo.h>  // Include Servo library

// Define pins for the sensors and motor control
const int trigPin = 13;             // Ultrasonic sensor trigger pin
const int echoPin = 12;             // Ultrasonic sensor echo pin
const int turbiditySensorPin = A1;  // Analog pin for turbidity sensor
const int gasSensorPin = A0;        // Analog pin for gas sensor
#define ONE_WIRE_BUS 2              // Pin for DS18B20 temperature sensor
const int pHSensorPin = A2;         // Analog pin for pH sensor

// Pin for the servo motor (for food dispensing)
const int servoPin = 7;             // Pin for the servo motor

// L298N Motor Driver pins
const int motor1Pin1 = 8;           // IN1 for Motor 1 (water drain)
const int motor1Pin2 = 9;           // IN2 for Motor 1
const int motor2Pin1 = 10;          // IN3 for Motor 2 (water fill)
const int motor2Pin2 = 11;          // IN4 for Motor 2

// Thresholds for turbidity sensor and temperature
const int clearThreshold = 65;          // Below this value, water is clear
const int cloudyThreshold = 95;         // Between this and clearThreshold, water is cloudy
const float temperatureThreshold = 30.0; // Temperature threshold to trigger motors

// Variables for sensor readings
long duration;
int distance, previousDistance = 9; // Store current and previous water level distances

// pH sensor variables
float calibration_value = 3.00; // Adjusted calibration value, test and adjust based on your sensor
unsigned long int avgval;
int buffer_arr[10];               // Buffer to hold readings
float ph_value = 0;               // Actual pH value

// Timer object to manage periodic sensor readings and food dispensing
SimpleTimer timer;

// Setup DS18B20 temperature sensor
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float temperature = 0; // Variable to store the temperature reading

// Servo object to control food dispensing
Servo foodServo;

// Global flag to indicate if temperature threshold is exceeded
bool tempExceeded = false;

/**
 * Function to control the motors using the L298N driver.
 * Motor 1: Drains water (dispose of water)
 * Motor 2: Fills water into the pond
 */
void controlMotors(bool motor1On, bool motor2On) {
  // Motor 1 (draining water)
  if (motor1On) {
    digitalWrite(motor1Pin1, HIGH); // Turn on motor 1 (dispose water)
    digitalWrite(motor1Pin2, LOW);  // Set motor direction
  } else {
    digitalWrite(motor1Pin1, LOW);  // Turn off motor 1
    digitalWrite(motor1Pin2, LOW);  // Stop motor 1
  }

  // Motor 2 (filling water)
  if (motor2On) {
    digitalWrite(motor2Pin1, HIGH); // Turn on motor 2 (fill water)
    digitalWrite(motor2Pin2, LOW);  // Set motor direction
  } else {
    digitalWrite(motor2Pin1, LOW);  // Turn off motor 2
    digitalWrite(motor2Pin2, LOW);  // Stop motor 2
  }
}

/**
 * Function to read temperature from the DS18B20 sensor and control motors based on temperature
 */
void read_temperature() {
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  // Check if the temperature exceeds the threshold
  if (temperature > temperatureThreshold) {
    if (!tempExceeded) {
      Serial.println("Temperature exceeded threshold! Activating motors.");
      tempExceeded = true;
      controlMotors(true, true);  // Turn on both motors (drain and fill water)
    }
  } else {
    if (tempExceeded) {
      Serial.println("Temperature below threshold. Deactivating motors.");
      tempExceeded = false;
      controlMotors(false, false);  // Turn off both motors
    }
  }
}

/**
 * Function to dispense food using the servo motor
 * Rotates servo 8 times between 0° and 180°, then turns off at 0°
 */
void dispense_food() {
  Serial.println("Activating automated feed system...");

  for (int i = 0; i < 8; i++) {
    // Rotate the servo to 180° (dispense food)
    foodServo.write(180);
    delay(1000);  // Wait for 1 second

    // Rotate the servo back to 0° (stop dispensing)
    foodServo.write(0);
    delay(1000);  // Wait for 1 second
  }

  Serial.println("Feed system complete.");
}

/**
 * Function to read pH values from the sensor and print whether it is acidic, neutral, or basic
 */
void read_pH() {
  // Read multiple analog values and store them in the buffer
  for (int i = 0; i < 10; i++) { 
    buffer_arr[i] = analogRead(pHSensorPin);  // Read from analog pin A2
    delay(30);  // Delay between readings
  }

  // Sort buffer values using bubble sort to filter out noise
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        int temp_val = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp_val;
      }
    }
  }

  // Average the middle 6 values to avoid noise
  avgval = 0;
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i];
  }

  avgval = avgval / 6;  // Get the average

  // Convert average to voltage
  float voltage = (float)avgval * (5.0 / 1024.0);

  // Use the voltage to pH conversion formula
  ph_value = 7 + ((2.5 - voltage) / calibration_value);

  // Ensure pH is within the expected range
  if (ph_value < 0) ph_value = 0;
  if (ph_value > 14) ph_value = 14;

  // Print the pH value and classification
  Serial.print("pH Value: ");
  Serial.println(ph_value);
  Serial.print("Voltage: ");
  Serial.println(voltage);

  if (ph_value < 7) {
    Serial.println("pH Condition: ACIDIC");
  } else if (ph_value == 7) {
    Serial.println("pH Condition: NEUTRAL");
  } else {
    Serial.println("pH Condition: BASIC");
  }
}

/**
 * Function to read turbidity values from the sensor
 */
void read_turbidity() {
  int sensorValue = analogRead(turbiditySensorPin);
  int turbidity = map(sensorValue, 0, 640, 100, 0);
  Serial.print("Turbidity Sensor Value: ");
  Serial.print(sensorValue);
  Serial.print(" | Turbidity Level: ");
  Serial.println(turbidity);
  
  if (turbidity < clearThreshold) {
    Serial.println("Water Condition: CLEAR");
  } else if (turbidity >= clearThreshold && turbidity <= cloudyThreshold) {
    Serial.println("Water Condition: CLOUDY");
  } else {
    Serial.println("Water Condition: DIRTY");
  }
}

/**
 * Function to read water level using ultrasonic sensor and compare with the previous level
 */
void read_water_level() {
  // Do not adjust water level if temperature threshold is exceeded
  if (tempExceeded) {
    // Temperature condition takes priority; do not adjust water level
    return;
  }

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH, 30000UL);

  if (duration == 0) {
    Serial.println("No echo received (timeout). Check wiring.");
  } else {
    distance = duration * 0.034 / 2;

    // Define water level thresholds (adjust as necessary)
    const int waterLevelThresholdLow = 10;   // Example lower threshold in cm
    const int waterLevelThresholdHigh = 20;  // Example upper threshold in cm

    // If the distance is below the lower threshold, add water
    if (distance < waterLevelThresholdLow) {
      Serial.println("Water level is too low. Adding water...");
      controlMotors(true, false);
        // Add water (turn on Motor 2)
    } 
    // If the distance is above the upper threshold, drain water
    else if (distance > waterLevelThresholdHigh) {
      Serial.println("Water level is too high. Draining water...");
      controlMotors(false, true);  // Drain water (turn on Motor 1)
    } 
    else {
      Serial.println("Water level is STABLE.");
      controlMotors(false, false);  // Turn off both motors
    }

    previousDistance = distance;
    Serial.print("Water Level Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
  }
}

/**
 * Function to read gas sensor values and convert to voltage
 */
void read_gas_sensor() {
  float voltage = getVoltage(gasSensorPin);
  Serial.print("Gas Sensor Voltage: ");
  Serial.println(voltage);
  
  if (voltage > 3.0) {
    Serial.println("Warning: High Gas Levels Detected!");
  } else {
    Serial.println("Gas Levels Normal");
  }
}

/**
 * Function to calculate the voltage from analog sensor value
 */
float getVoltage(int pin) {
  return (analogRead(pin) * 0.004882814);  // Convert analog reading to voltage
}

/**
 * Setup function to initialize sensors, motor control, and serial communication
 */
void setup() {
  Serial.begin(9600);

  // Initialize the pins for ultrasonic sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Initialize DS18B20 temperature sensor
  sensors.begin();

  // Initialize the pins for motor control using L298N
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);

  // Initialize the servo motor
  foodServo.attach(servoPin);    // Attach the servo to the pin
  foodServo.write(0);            // Set initial position to 0 degrees

  Serial.println("Sensor Readings and Motor Control Started");

  // Set a timer to read and display pH value every 500 ms
  timer.setInterval(500L, read_pH);

  // Set a timer to dispense food every 5 minutes (300000 milliseconds)
  timer.setInterval(60000L, dispense_food);  // 5 minutes
}

/**
 * Main loop function that continuously reads sensor values and controls motors
 */
void loop() {
  read_temperature();  // This will also control the motors based on temperature
  read_turbidity();
  read_water_level();  // This will maintain water level if tempExceeded is false
  read_gas_sensor();
  
  // Timer to execute the pH reading and food dispensing at the set intervals
  timer.run();
  
  delay(5000);  // 5 seconds delay between each full cycle of readings
}
