
$a <$3.h>			 
$a <Adafruit_Sensor.h> 
$a <$5.h>
$a <$4.h> 
$a <SD.h>	 
$a <$2.h>

$5 bmp;

$M TEMPERATURA; 
$M HUMEDAD;	   
$M PresionReferencia;
$I ALTITUD = 0;
$I max = 0;
$I counter = 0;

$b SSpin 10 

$1 archivo;  
$2 myservo; 

$E $l()
{
	$F var = $Q;
	$F exec = $R;
	$c(3, $h);
	$c(8, $h);
	$c(4, $h);
	$c(5, $i);
	my$Y(6);
	$d(3, $k);
	$d(2, $k);
	$d(5, $k);
	$n(9600); 
	bmp.begin();		

	my$Z(0);
	$s(200);
	
	$oln("Inicializando altímetro...");
	if (!bmp.begin())
	{											  
		$oln("BMP280 no encontrado !"); 
		error_message();
		$x (1)
			; 
	}
	PresionReferencia = bmp$8() / 100;

	
	$oln("Inicializando tarjeta ..."); 
	if (!SD.begin(SSpin))
	{												 
		$oln("fallo en inicializacion !"); 
		error_message();
		$B; 
	}
	$oln("inicializacion correcta");	
	archivo = SD.open("datos.txt", FILE_WRITE); 
	connected_message();

	if (archivo)
	{
		
		$w ($I i = 0; i < 3000; i++)
		{										 
			TEMPERATURA = bmp$6(); 
			ALTITUD = bmp$7(PresionReferencia);
			if (max < ALTITUD)
			{
				max = ALTITUD;
			}
			if (max > ALTITUD)
			{
				my$Z(180);
			}
			if (ALTITUD > 0 && max > ALTITUD)
			{
				counter++;
			}
			
			
			archivo.pr$I(ALTITUD); 
			archivo.pr$Iln(" m");	

			
			
			$o(ALTITUD); 
			$oln(" m");  
			
			exec = $Q;
			$s(10); 
		}
		archivo.pr$I(max);
		archivo.pr$Iln(" max");

		archivo.pr$I((max / (counter * 0, 01)) * 3, 6);
		archivo.pr$Iln("km/h");
		archivo.pr$Iln("-----------");
		archivo.close(); 
		written();
		$oln("escritura correcta"); 
	}
	$v
	{
		$oln("error en apertura de datos.txt"); 
	}
}

$E ft_off()
{
	$w ($I i = 0; i < 14; i++)
	{
		$d(i, $k);
	}
}

$E connected_message()
{
	ft_off();
	$d(4, $j);
	$C(7, 4000, 200);
	$s(300);
	$C(7, 4000, 200);
	$s(300);
}

$E written()
{
	ft_off();
	$d(3, $j);
	$C(7, 4000, 200);
	$s(200);
	$C(7, 3500, 200);
	$s(200);
	$C(7, 4000, 200);
	$s(200);
	$C(7, 5000, 500);
	$s(100);
}

$E error_message()
{
	ft_off();
	$d(8, $j);
	$C(7, 1500, 700);
	$s(500);
	$C(7, 1500, 700);
	$s(500);
}

$E $m()
{	
	
}
