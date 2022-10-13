/*
 * File:   proyectoFinal.c INCUBADORA
 * Author: Hernandez Camacho Jose Luis
 *         Rivera Martinez Juan Carlos
 *
 * Created on 16 de octubre de 2021, 03:58 PM
 */
#pragma config FOSC = INTOSCIO // Oscillator Selection (Internal oscillator)
#pragma config WDTEN = OFF // Watchdog Timer Enable bits (WDT disabled in hardware (SWDTENignored))
#pragma config MCLRE = ON // Master Clear Reset Pin Enable (MCLR pin enabled; RE3 input disabled)
#pragma config LVP = OFF // Single-Supply ICSP Enable bit (Single-Supply ICSP disabled)
#pragma config ICPRT = OFF // Dedicated In-Circuit Debug/Programming Port Enable (ICPORT disabled)
#include <xc.h>
#include <stdio.h>
#include <pic18f45k50.h>

#define _XTAL_FREQ 8000000// Frecuencia por default
char dias=0,horas=0,minutos=0,segundos = 0;
char servoPos = 0;
char estadoEEPROM; 

//Bits de control
// T1CONbits.TMR1ON = 0;
// T0CONbits.TMR0ON = 0;
//  Empieza LCD //
void putch(char data) {     //la manda a llamar printf como 8 bits
    char Activa;
    Activa = data & 0xF0;   //deja los 4 bits mas significativos xxxx0000
    LATD = Activa | 0x05; //0bxxxx0101  con x05 los 4 bits mas significativos no cambian
                        //cuando ponen el pin 0001 significa que mandan un dato
                        //si ponen 0000 mandan un comando
                        //0101 el bit 2 sirve para activar el Enable
    __delay_us(10);
    LATD = Activa | 0x01; //0bxxxx0001  
    __delay_ms(1);
    Activa = data << 4;
    LATD = Activa | 0x05;
    __delay_us(10);
    LATD = Activa | 0x01;    
}

void putcm(char data) { //Putcm es para mandar comandos
    char Activa;
    Activa = data & 0xF0;
    LATD = Activa | 0x04;   
    __delay_us(10);
    LATD = Activa;
    __delay_ms(1);
    Activa = data << 4;
    LATD = Activa | 0x04;
    __delay_us(10);
    LATD = Activa;
}

void InicializaLCD(void){
__delay_ms(30);
putcm(0x02);    // Inicializa en modo 4 bits
__delay_ms(1);

putcm(0x28);    // Inicializa en 2 líneas 5x7
__delay_ms(1);

putcm(0x2C);
__delay_ms(1);

putcm(0x0C);
__delay_ms(1);

putcm(0x06);
__delay_ms(1);

putcm(0x80); //Posiciona el curson en 1,1
__delay_ms(1);
}
//  TERMINA LCD //
void EscribeEEprom (char Direccion, char Valor){ 
    EEADR = Direccion; 
    EEDATA = Valor; 
 
    EECON1bits.WREN = 1; 
    EECON2 = 0x55; 
    EECON2 = 0xAA; 
    EECON1bits.WR=1; 
    while(EECON1bits.WR); 
    EECON1bits.WREN = 0; 
}  

char LeeEEprom (char Direccion){ 
    EEADR = Direccion; 
    EECON1bits.RD = 1; 
    while (EECON1bits.RD); //Espera a que termine de escibir 
    return EEDATA; 
}

void tempHumCheck(char chumEnt,char chumDec, char ctempEnt, char ctempDec){
    //Rango de temperatura adecuado para incubacion max 38º min 36º
    //Rango de humedad adecuada para incubacion max 55 min 60 ultimos tres dias a 65
        
    if(ctempEnt+(ctempDec/100) < 36.0){//Encender lampara 
        LATAbits.LA5 = 1;
    }else if(ctempEnt+(ctempDec/100) >38.0)//Apagar lampara
        LATAbits.LA5 =0;
    
    if((chumEnt+(chumDec/100) > 60.0) && (dias<=18) ){ //Encender ventilador para los primeros 18 dias
        LATAbits.LA6 = 1;
    }else if((chumEnt+(chumDec/100) < 55.0) && (dias<=18)){
        LATAbits.LA6 =0;
    }else if((chumEnt+(chumDec/100) < 63.0) && (dias>18)){ // Encender ventilador para los ultimos 3 dias
        LATAbits.LA6=0;
    }else if((chumEnt+(chumDec/100) > 65.0) && (dias>18)){
        LATAbits.LA6=1;
    }
    
}

void Configuracion(void){
    TRISD=0x00;
    ANSELD=0x00;
    TRISA=0b00001110; //RA0 como salida   
    ANSELA=0x00;  //RA0 como digital
    TRISB=0x00; //canal B son salidas digitales
    ANSELB=0x00;
    TRISC=0x00;
    ANSELC=0x00;
    T0CON = 0b10000111; //prescaler 1:256
    INTCON = 0b10100000;//Habilitar interrupciones TMR0
    RCONbits.IPEN=0; //Desabilita interrupciones 
    OSCCON = 0b01100011;
    
    T1CON = 0b00110111;  //timer FOSC, prescale=8, secon oscilator off, 16bits, enable timer1
    //20ms  -   0°
    TMR1L=0x77;
    TMR1H=0xEC;
    PIE1=0b00000001;
    //PIR1=0b00000000; // BORRAR REGISTRO
    
    // Para la escritura de la EEPROM
    EECON1bits.EEPGD = 0; //Selecciona memoria de datos 
    EECON1bits.CFGS = 0; // Selecciona memoria EEPROM 
    
}

void iniciarDHT11(void){ // Funcion para la conversion A/D
    char i;
    char humEnt=0, humDec=0, tempEnt=0, tempDec=0, checksum=0;
    
    
    TRISAbits.RA0 = 0;
    LATAbits.LA0 = 0; // Pulso bajo por 20ms
    __delay_ms(20);
    LATAbits.LA0 = 1; //Pulso alto por 20us
    __delay_us(20);   
    TRISAbits.RA0 = 1; //Pin como entrada  
   //Señal de respuesta 
    
    __delay_us(40);
    if(PORTAbits.RA0 == 0 ){
        __delay_us(60);
        
        if(PORTAbits.RA0 == 1){
            __delay_us(75);
        
    //while(!PORTAbits.RA0);
    //while(PORTAbits.RA0);

           //Lectura de humedad entera
            
            for(i=0;i<8;i++){
                while(!PORTAbits.RA0);
                __delay_us(30);
                if(PORTAbits.RA0){
                    humEnt = humEnt<<1 | 1;
                }else{
                    humEnt = humEnt<<1;

                }
                while(PORTAbits.RA0);
            }
           
           //Lectura de humedad decimal
           for(i=0;i<8;i++){
               while(!PORTAbits.RA0);
                __delay_us(30);
                if(PORTAbits.RA0){
                    humDec = humDec<<1 | 1;
                }else{
                    humDec = humDec<<1;
                }
                while(PORTAbits.RA0);
            }
           //Lectura de temperatura entera
           for(i=0;i<8;i++){
                while(!PORTAbits.RA0);
                __delay_us(30);
                if(PORTAbits.RA0){
                    tempEnt = tempEnt<<1 | 1;
                }else{
                    tempEnt = tempEnt<<1;

                }
                while(PORTAbits.RA0);
            }
           //Lectura de temperatura decimal
           for(i=0;i<8;i++){
                while(!PORTAbits.RA0);
                __delay_us(30);
                if(PORTAbits.RA0){
                    tempDec = tempDec<<1 | 1;
                }else{
                    tempDec = tempDec<<1;
                }
                while(PORTAbits.RA0);
            }
           //Checksum
           for(i=0;i<8;i++){
                while(!PORTAbits.RA0);
                __delay_us(30);
                if(PORTAbits.RA0){
                    checksum = checksum<<1 | 1;
                }else{
                    checksum = checksum<<1;
                }
                while(PORTAbits.RA0);
            }
           if(checksum == (humEnt+humDec+tempEnt+tempDec)){
                putcm(0xC0);
                __delay_ms(1);
                printf("H:%d.%d  T:%d.%d",humEnt,humDec,tempEnt,tempDec);
                tempHumCheck(humEnt, humDec, tempEnt, tempDec);
                
           }
    
       }
    } // fIN DE LOS IFS DE COMPROBACION
}

void girarHuevos(void){
    //Activar el TMR1 y desactivar TMR0
    
    T0CONbits.TMR0ON = 0;
    servoPos +=1;
    
    T1CONbits.TMR1ON = 1;
    __delay_ms(30);
    T1CONbits.TMR1ON = 0;
    if(servoPos > 2){
        servoPos = 0;
    }
    T0CONbits.TMR0ON = 1;
    
    
    
    
}
void tiempo(void){
    if(segundos >= 60){
        
        putcm(0x01);
        __delay_ms(5);
        minutos += 1;
        segundos = 0;
        if(minutos >=60){
            horas += 1;
            minutos = 0;
            //Rutina para volteo de huevos
            girarHuevos();
            
            EscribeEEprom(1,minutos);
            EscribeEEprom(2,horas);
            EscribeEEprom(3,dias);
            
            
            
            if(horas >= 24){
                dias += 1;
                horas = 0;
            }
        }
    }
    
    putcm(0x80);    //Para poner el cursor en la posicion 1
    __delay_ms(1);
    printf("%d:%d:%d:%d",dias,horas,minutos,segundos);
    
    iniciarDHT11();
    
}

void __interrupt(high_priority) Inter(void){
    if(INTCONbits.TMR0IF == 1){
        segundos+=1; 
        INTCONbits.TMR0IF=0;
        TMR0L = 0x7C;
        TMR0H = 0xE1;
    }   
    if(PIR1bits.TMR1IF==1){  

        if(servoPos==0){  //Cuando hay un 1
            LATAbits.LATA4=1;
            __delay_ms(1.25);
            LATAbits.LATA4=0;
            
            PIR1bits.TMR1IF=0;
            
            //Para -45º
            TMR1L=0xAF;
            TMR1H=0xED;
            
            
        }else if(servoPos == 1){
            LATAbits.LATA4=1;
            __delay_ms(1.5);
            LATAbits.LATA4=0;
            PIR1bits.TMR1IF=0;
            //Para 0º
            
            TMR1L=0x71;
            TMR1H=0xED;
            
            
        }else{   
            LATAbits.LATA4=1;
            __delay_ms(1.75);
            LATAbits.LATA4=0;
            PIR1bits.TMR1IF=0;
            //Para 45º
            
            TMR1L=0x2C;
            TMR1H=0xEE;
            
            
            }
        
    }
}
void main(void) {
    __delay_ms(500);
    Configuracion();
    T0CONbits.TMR0ON = 0;
    T1CONbits.TMR1ON = 0;
    InicializaLCD();
   
   
    TMR0L = 0x7C;
    TMR0H = 0xE1;
    TMR1L=0x77;
    TMR1H=0xEC;
    char bandera=0;
    
    estadoEEPROM = LeeEEprom(0); // Si es 1 habia una rutina en proceso
    if(estadoEEPROM == 1){
        minutos = LeeEEprom(1);
        horas = LeeEEprom(2);
        dias = LeeEEprom(3); 
        bandera=1;
        T0CONbits.TMR0ON = 1;
    }
    
    while(1){
        //Menu de opciones para el sistema
        LATCbits.LC0^=1;
        //tiempo(); 
        if((PORTAbits.RA1 == 0) && (bandera != 1) ){ // Se presiona INICIO
            //while(PORTAbits.RA1 == 0);
            
            bandera = 1;
            putcm(0x01);
            __delay_ms(5);
            putcm(0x80);
            __delay_ms(1);
            printf(" INICIANDO ");
            putcm(0xC0);
            __delay_ms(1);
            printf("INCUVACION");
            __delay_ms(1000);
            putcm(0x01);
            __delay_ms(5);
            //RCONbits.IPEN=1;
            T0CONbits.TMR0ON = 1;
            estadoEEPROM = LeeEEprom(0);
            EscribeEEprom(0,1);
            if(estadoEEPROM == 0){
                EscribeEEprom(0,1);
                EscribeEEprom(1,0);
                EscribeEEprom(2,0);
                EscribeEEprom(3,0);
            }
            
        }else if((PORTAbits.RA2 == 0) && (bandera == 1)){ //Se presiona PAUSA
            //while(PORTAbits.RA2 == 1);
            bandera = 2;
            //RCONbits.IPEN = 0;
            T0CONbits.TMR0ON = 0;
            putcm(0x01);
            __delay_ms(5);
            EscribeEEprom(1,minutos);
            EscribeEEprom(2,horas);
            EscribeEEprom(3,dias);
            
            
        }else if(PORTAbits.RA3 == 0  && (bandera == 1 || bandera == 2)){ // Se presiona DETENER
            //while(PORTAbits.RA3 == 1);
            bandera = 3;
            T0CONbits.TMR0ON = 0;
            //limpiar tiempos
            horas =0;
            minutos=0;
            dias=0;
            segundos=0;
            EscribeEEprom(0,0);
            EscribeEEprom(1,0);
            EscribeEEprom(2,0);
            EscribeEEprom(3,0);
            
        }
        
        switch(bandera){
            case 0: //sin iniciar
                //putcm(0x01);
                //__delay_ms(1);
                putcm(0x80);
                __delay_ms(1);
                printf("Presiona INICIO ");
                putcm(0xC0);
                __delay_ms(1);
                printf("Para comenzar   ");
                break;
            case 1: // Ciclo activo
                tiempo();
                if(dias == 21 ){
                    T0CONbits.TMR0ON = 0;//Apaga timer
                    bandera=0;
                    putcm(0x01);
                    __delay_ms(5);
                    putcm(0x80);
                    __delay_ms(1);
                    printf("INCUBACION");
                    putcm(0xC0);
                    __delay_ms(1);
                    printf("FINALIZADA");
                    //Limpiar EEPROM
                    EscribeEEprom(0,0);
                    dias=0;
                    horas=0;
                    minutos=0;
                    segundos=0;
                    while(PORTAbits.RA3 == 0);
                    
                    
                }
                break;
            case 2://Ciclo en pausa
                putcm(0x80);
                __delay_ms(1);
                printf(" Ciclo en Pausa ");
                putcm(0xC0);
                __delay_ms(1);
                printf("Pulsa INICIO");
                
                break;
            case 3:
                putcm(0x01);
                __delay_ms(5);
                putcm(0x80);
                __delay_ms(1);
                printf("Ciclo Cancelado");
                putcm(0xC0);
                __delay_ms(1);
                printf("Saliendo");
                __delay_ms(2000);
                bandera = 0;
                
                break;
        
        }
        
        __delay_ms(1000);
    }
    
    return;
}
