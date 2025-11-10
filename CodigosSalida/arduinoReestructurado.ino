
#include <Wire.h>			 
#include <Adafruit_Sensor.h> 
#include <Adafruit_BMP280.h>
#include <SPI.h> 
#include <SD.h>	 
#include <Servo.h>

Adafruit_BMP280 bmp;

float TEMPERATURA; 
float HUMEDAD;	   
float PresionReferencia;
int ALTITUD = 0;
int max = 0;
int counter = 0;

#define SSpin 10 

File archivo;  
Servo myservo; 

void setup()
{
	bool var = true;
	bool exec = false;
	pinMode(3, OUTPUT);
	pinMode(8, OUTPUT);
	pinMode(4, OUTPUT);
	pinMode(5, INPUT);
	myservo.attach(6);
	digitalWrite(3, LOW);
	digitalWrite(2, LOW);
	digitalWrite(5, LOW);
	Serial.begin(9600); 
	bmp.begin();		

	myservo.write(0);
	delay(200);
	
	Serial.println("Inicializando altímetro...");
	if (!bmp.begin())
	{											  
		Serial.println("BMP280 no encontrado !"); 
		error_message();
		while (1)
			; 
	}
	PresionReferencia = bmp.readPressure() / 100;

	
	Serial.println("Inicializando tarjeta ..."); 
	if (!SD.begin(SSpin))
	{												 
		Serial.println("fallo en inicializacion !"); 
		error_message();
		return; 
	}
	Serial.println("inicializacion correcta");	
	archivo = SD.open("datos.txt", FILE_WRITE); 
	connected_message();

	if (archivo)
	{
		
		for (int i = 0; i < 3000; i++)
		{										 
			TEMPERATURA = bmp.readTemperature(); 
			ALTITUD = bmp.readAltitude(PresionReferencia);
			if (max < ALTITUD)
			{
				max = ALTITUD;
			}
			if (max > ALTITUD)
			{
				myservo.write(180);
			}
			if (ALTITUD > 0 && max > ALTITUD)
			{
				counter++;
			}
			
			
			archivo.print(ALTITUD); 
			archivo.println(" m");	

			
			
			Serial.print(ALTITUD); 
			Serial.println(" m");  
			
			exec = true;
			delay(10); 
		}
		archivo.print(max);
		archivo.println(" max");

		archivo.print((max / (counter * 0, 01)) * 3, 6);
		archivo.println("km/h");
		archivo.println("-----------");
		archivo.close(); 
		written();
		Serial.println("escritura correcta"); 
	}
	else
	{
		Serial.println("error en apertura de datos.txt"); 
	}
}

void ft_off()
{
	for (int i = 0; i < 14; i++)
	{
		digitalWrite(i, LOW);
	}
}

void connected_message()
{
	ft_off();
	digitalWrite(4, HIGH);
	tone(7, 4000, 200);
	delay(300);
	tone(7, 4000, 200);
	delay(300);
}

void written()
{
	ft_off();
	digitalWrite(3, HIGH);
	tone(7, 4000, 200);
	delay(200);
	tone(7, 3500, 200);
	delay(200);
	tone(7, 4000, 200);
	delay(200);
	tone(7, 5000, 500);
	delay(100);
}

void error_message()
{
	ft_off();
	digitalWrite(8, HIGH);
	tone(7, 1500, 700);
	delay(500);
	tone(7, 1500, 700);
	delay(500);
}

void loop()
{	
	
}
