/*---------------------------------------------------------------------------------------------------------*/
/*                                                                                                         */
/* Copyright (c) Nuvoton Technology Corp. All rights reserved.                                             */
/*                                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/

#include <stdio.h>
#include "NUC1xx.h"
#include "Driver\DrvSYS.h"
#include "Driver\DrvGPIO.h"
#include "Driver\DrvI2C.h"
#include "EEPROM_24LC64.h"
#include "LCD_Driver.h"
#include "Seven_Segment.h"
#include "ScanKey.h"

#define DELAY_D 120
#define PAGE_SIZE 16
#define BASE_ADDRESS 0
#define FIRST_ADDRESS 1
/*--------------------------------------------------------------------------
	BAGIAN DATA
	--------------------------------------------------------------------------*/
char TabelPass[64][4];
unsigned char CHAR_TAB[17] = {'G','0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
uint32_t START_ADDRESS;
int CurrentPage,PageSize;
int PassSize,CurrPassSize;
int temp,temp0,temp1,temp2,temp3;
void delay_loop(void)
 {
 uint32_t i,j;
	for(i=0;i<3;i++)	
	{
		for(j=0;j<300;j++);
    }
 
 }

/*----------------------------------------------------------------------------
  Interrupt subroutine
  ----------------------------------------------------------------------------*/
static unsigned char count=0;
static unsigned char loop=12;
void TMR0_IRQHandler(void) // Timer0 interrupt subroutine 
{ 
    unsigned char i=0;
 	TIMER0->TISR.TIF =1;
	count++;
	if(count==5)
	{
	   	DrvGPIO_ClrBit(E_GPC,loop);
	   	loop++;
	   	count=0;
	   	if(loop==17)
	   	{
	   		for(i=12;i<16;i++)
		   	{
	   			DrvGPIO_SetBit(E_GPC,i);	   
	   		}
			loop=12;
	   }
	}
}

void Timer_initial(void)
{
	/* Step 1. Enable and Select Timer clock source */          
	SYSCLK->CLKSEL1.TMR0_S = 0;	//Select 12Mhz for Timer0 clock source 
  SYSCLK->APBCLK.TMR0_EN =1;	//Enable Timer0 clock source

	/* Step 2. Select Operation mode */	
	TIMER0->TCSR.MODE=1;		//Select periodic mode for operation mode

	/* Step 3. Select Time out period = (Period of timer clock input) * (8-bit Prescale + 1) * (24-bit TCMP)*/
	TIMER0->TCSR.PRESCALE=0;	// Set Prescale [0~255]
	TIMER0->TCMPR  = 1000000;		// Set TICR(TCMP) [0~16777215]
								// (1/22118400)*(0+1)*(2765)= 125.01usec or 7999.42Hz

	/* Step 4. Enable interrupt */
	TIMER0->TCSR.IE = 1;
	TIMER0->TISR.TIF = 1;		//Write 1 to clear for safty		
	NVIC_EnableIRQ(TMR0_IRQn);	//Enable Timer0 Interrupt

	/* Step 5. Enable Timer module */
	TIMER0->TCSR.CRST = 1;		//Reset up counter
	TIMER0->TCSR.CEN = 1;		//Enable Timer0

  TIMER0->TCSR.TDR_EN=1;		// Enable TDR function
}

void WriteEEPROM(int Data,uint32_t Address){
		DrvGPIO_InitFunction(E_FUNC_I2C1);
		Write_24LC64(0x00000000+Address,Data);
}
int LoadEEPROM(uint32_t Address){
	uint8_t CurrentData;
	DrvGPIO_InitFunction(E_FUNC_I2C1);
	CurrentData=Read_24LC64(0x00000000+Address);
	return CurrentData;
}
int LoadBase(){
	uint8_t CurrentData;
	DrvGPIO_InitFunction(E_FUNC_I2C1);
	CurrentData=Read_24LC64(0x00000000);
	return CurrentData;
}	

void Load_Password(uint32_t Address, int PassSize){
		int i,j;
		int Data[4];
		for (i=0;i<PassSize;i++){
				for (j=0;j<4;j++){
					Data[j]=LoadEEPROM(i*4+Address+j);
					TabelPass[i][j]=CHAR_TAB[Data[j]];
				}
		}
}			

void DEBUG(int Data,int noline){
	char line[16]="                ";
	int i,j,k;
	print_lcd(noline,line);
	line[0]='D';
	line[1]='A';
	line[2]='T';
	line[3]='A';
	line[4]='=';
	sprintf(line+5,"%d",Data);
	print_lcd(noline,line);}
void Display_Password(int CurrPassSize){
	char line[4][16];
	int i,j;
	for (j=0;j<4;j++){
		for (i=0;i<16;i++){
			line[j][i]=' ';}
	}
	for (i=0;i<CurrPassSize;i++){
			int Idx=i/2;
			if (i%2==0){
					line[Idx][0]=TabelPass[i][0];
					line[Idx][1]=TabelPass[i][1];
					line[Idx][2]=TabelPass[i][2];
					line[Idx][3]=TabelPass[i][3];
			}
			else{
					line[Idx][5]=TabelPass[i][0];
					line[Idx][6]=TabelPass[i][1];
					line[Idx][7]=TabelPass[i][2];
					line[Idx][8]=TabelPass[i][3];}
			line[Idx][4]=' ';
			line[Idx][9]='\0';
	}
//	print_lcd(0,"Kunci Tersimpan : ");

	if (CurrPassSize>0){
		print_lcd(0,line[0]);
		print_lcd(1,line[1]);
		print_lcd(2,line[2]);}
}

void clear_screen(){
	char line[16]="                ";
	print_lcd(0,line);
	print_lcd(1,line);
	print_lcd(2,line);
	print_lcd(3,line);
}
int main(void)
{
	 int i=0,j=0,k=0,l=0,m=1,n=0,temp,refresh;
	 int Segment[4];
	 char line1[16]="                ";
	 char line2[16]="                ";
	 char line3[16];
	 char line4[16];
	 int CurrentValue=0;
	 int CurrentPos=0;
	 int FirstPress=-1;
	 int KeyPressed=0;
	 int Delay_Device=0;
	 int PrevPress=0;
	 uint8_t testkey;
	 uint32_t CurrAddress;
	 int Data[4];
	 char Pass[16];
	 uint8_t CurrentData;
	/* Unlock the protected registers */	
	UNLOCKREG();
   	/* Enable the 12MHz oscillator oscillation */
	DrvSYS_SetOscCtrl(E_SYS_XTL12M, 1);
 
     /* Waiting for 12M Xtal stalble */
    SysTimerDelay(5000);
 
	/* HCLK clock source. 0: external 12MHz; 4:internal 22MHz RC oscillator */
	DrvSYS_SelectHCLKSource(0);		
    /*lock the protected registers */
	LOCKREG();				

	DrvSYS_SetClockDivider(E_SYS_HCLK_DIV, 0); /* HCLK clock frequency = HCLK clock source / (HCLK_N + 1) */

    for(i=12;i<16;i++)
		{		
			DrvGPIO_Open(E_GPC, i, E_IO_OUTPUT);
    }

	Initial_pannel();  //call initial pannel function
	clr_all_pannal();
	   	  
	Timer_initial();
	Segment[0]=0;Segment[1]=0;Segment[2]=0;Segment[3]=0;
	OpenKeyPad();
	//PassSize=LoadBase();
	print_lcd(0,"Reset EEPROM?");
	print_lcd(1,"Tekan 1 ya");
	print_lcd(2,"Tekan 2 no");
	do{
		testkey=Scankey();
	}while(testkey!=1 && testkey!=2);
	
	if (testkey==1){
		while(testkey!=0){
				testkey=Scankey();
		}
		WriteEEPROM(0,0);
		PassSize=0;
clear_screen();}
	else if (testkey==2){
		while(testkey!=0){
				testkey=Scankey();
	}
		clear_screen();
		PassSize=LoadBase();}
	/*if (PassSize>4){
		CurrPassSize=4;
		Load_Password(1,4);}
	else{
			//Size pass
			CurrPassSize=PassSize;
			Load_Password(1,PassSize);}*/
	START_ADDRESS=PassSize*4+1;
	//DEBUG(0,PassSize);
	//DEBUG(1,START_ADDRESS);
	//HALAMAN
	CurrentPage=0;
	PageSize=0;
	PageSize+=PassSize/6;
	

	Load_Password((CurrentPage*16+1),(PassSize-CurrentPage*4));
	Display_Password((PassSize-CurrentPage*4));
	Segment[0]=0;Segment[1]=0;Segment[2]=0;Segment[3]=0;
	CurrentPos=0;
	while(1){
				testkey=Scankey();
				if (testkey==1){
					while(testkey!=0){
						testkey=Scankey();}
						n=0;
					if (FirstPress==-1){
								FirstPress=1;}
					if (Delay_Device>190 && (FirstPress==1)){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}
						FirstPress=-1;}
					else if (FirstPress!=1){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}}
					else{
						Delay_Device=0;
					}
					CurrentValue++;
					if (CurrentValue>2){
						CurrentValue=1;
					}
					FirstPress=1;
					Segment[CurrentPos]=CurrentValue;
				}else if (testkey==2){
					while(testkey!=0){
						testkey=Scankey();}
					n=0;
					if (FirstPress==-1){
								FirstPress=2;}
					if (Delay_Device>190 && (FirstPress==2)){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}
						FirstPress=-1;}
					else if (FirstPress!=2){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}}
					else{
						Delay_Device=0;
					}
					CurrentValue++;
					if(!(CurrentValue>=3 && CurrentValue<=5)){
						CurrentValue=3;
					}
					FirstPress=2;
					Segment[CurrentPos]=CurrentValue;
				}else if (testkey==3){
					while(testkey!=0){
						testkey=Scankey();}
					n=0;
					if (FirstPress==-1){
						FirstPress=3;}
					if (Delay_Device>190 && (FirstPress==3)){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}
						FirstPress=-1;}
					else if (FirstPress!=3){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}}
					else{
						Delay_Device=0;
					}
					CurrentValue++;
					if(!(CurrentValue>=6 && CurrentValue<=8)){
						CurrentValue=6;
					}
					FirstPress=3;
					Segment[CurrentPos]=CurrentValue;
				}else if (testkey==4){
					while(testkey!=0){
						testkey=Scankey();}
					n=0;
						if (FirstPress==-1){
						FirstPress=4;}
					if (Delay_Device>190 && (FirstPress==4)){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}
						FirstPress=-1;}
					else if (FirstPress!=4){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}}
					else{
						Delay_Device=0;
					}
					CurrentValue++;
					if(!(CurrentValue>=9 && CurrentValue<=10)){
						CurrentValue=9;
					}
					FirstPress=4;
					Segment[CurrentPos]=CurrentValue;
				}else if (testkey==5){ 
					while(testkey!=0){
						testkey=Scankey();}
					n=0;
						if (FirstPress==-1){
						FirstPress=5;}
					if (Delay_Device>190 && (FirstPress==5)){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}
						FirstPress=-1;}
					else if (FirstPress!=5){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}}
					else{
						Delay_Device=0;
					}
					CurrentValue++;
					if(!(CurrentValue>=11 && CurrentValue<=13)){
						CurrentValue=11;
					}	
					FirstPress=5;
					Segment[CurrentPos]=CurrentValue;
				}else if (testkey==6){
					while(testkey!=0){
						testkey=Scankey();}
					n=0;
						if (FirstPress==-1){
						FirstPress=6;}
					if (Delay_Device>190 && (FirstPress==6)){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}
						FirstPress=-1;}
					else if (FirstPress!=6){
						Delay_Device=0;
						CurrentPos++;
						if (CurrentPos>3){
							CurrentPos=3;}}
					else{
						Delay_Device=0;
					}
					CurrentValue++;
					if(!(CurrentValue>=14 && CurrentValue<=16)){
						CurrentValue=14;
					}
					FirstPress=6;
					Segment[CurrentPos]=CurrentValue;
				}
				else if (testkey==7){
					while(testkey!=0){
						testkey=Scankey();}
					n=0;
					Segment[CurrentPos]=0;
					if (CurrentPos>0){
						CurrentPos--;
						Delay_Device=191;
						}else{
							Delay_Device=0;
						}
						FirstPress=-1;
						CurrentValue=Segment[CurrentPos];					
				}
				else if (testkey==8){
					while(testkey!=0){
						testkey=Scankey();}
				
					if (Segment[0]!=0 && Segment[1]!=0 && Segment[2]!=0 && Segment[3]!=0 ){
						
							i=0;
							for (i = 0;i<PassSize;i++){ //cek apakah password tsb sudah tersimpan
								l=0;
								m=0;
								for (l=0;l<4;l++){
									temp = LoadEEPROM((i*4) + l+1);
									if (temp!=Segment[l]){
										m = 1;
										break;
									}
								}
								if (m==0){//password sudah ada tersimpan
									break;
								}
							}
							if (m!=0){
								clear_screen();
								print_lcd(0,"tersimpan");
								for (i=0;i<1000000;i++);
								clear_screen();
								i=0;
								for (CurrAddress=START_ADDRESS;CurrAddress<START_ADDRESS+4;CurrAddress++){
									WriteEEPROM(Segment[i],CurrAddress);
									i++;}
								//for (CurrAddress=0;CurrAddress<4;CurrAddress++){
								//	Data[CurrAddress]=LoadEEPROM(CurrAddress);}	
								PassSize++;
								if (PassSize%6==1 && PassSize!=1){
									PageSize++;
								}
								WriteEEPROM(PassSize,0);
								START_ADDRESS=START_ADDRESS+4;
								Load_Password((CurrentPage*24+1),(PassSize-CurrentPage*6));
								Display_Password((PassSize-CurrentPage*6));
								Segment[0]=0;Segment[1]=0;Segment[2]=0;Segment[3]=0;
								CurrentPos=0;
								Delay_Device=0;
								FirstPress=-1;
						}else{
							clear_screen();
							print_lcd(0,"sudah ada");
							for (i=0;i<1000000;i++);
							clear_screen();
							Load_Password((CurrentPage*24+1),(PassSize-CurrentPage*6));
							Display_Password((PassSize-CurrentPage*6));		
							Segment[0]=0;Segment[1]=0;Segment[2]=0;Segment[3]=0;
							CurrentPos=0;
							Delay_Device=0;
							FirstPress=-1;							
						}
					}
					else{
						if (Segment[0]==0 && Segment[1]==0 && Segment[2]==0 && Segment[3]==0 ){
						
							n++;
							if (n>=2){
								
								WriteEEPROM(0,0);
								PassSize=0;
								clear_screen();	
								print_lcd(0,"reset");
								for (i=0;i<1000000;i++);
								clear_screen();
								n=0;
								PageSize=0;
								CurrentPage=0;
								START_ADDRESS=1;
								Load_Password((CurrentPage*24+1),(PassSize-CurrentPage*6));
								Display_Password((PassSize-CurrentPage*6));
							}
						}else{
							temp =CurrentPos;
							temp0=Segment[0];temp1=Segment[1];temp2=Segment[2];temp3=Segment[3];
							clear_screen();
							print_lcd(0,"belum benar");							
							for (i=0;i<1000000;i++);
							clear_screen();
							Load_Password((CurrentPage*24+1),(PassSize-CurrentPage*6));
							Display_Password((PassSize-CurrentPage*6));
							CurrentPos=temp;
							Segment[0]=temp0;Segment[1]=temp1;Segment[2]=temp2;Segment[3]=temp3;
						
						}
					}
				}
				else if (testkey==9){
						while(testkey!=0){
							testkey=Scankey();}
						temp=CurrentPos;
						temp0=Segment[0];temp1=Segment[1];temp2=Segment[2];temp3=Segment[3];
						n=0;
						CurrentPage++;
						if (CurrentPage>PageSize){
							CurrentPage=0;}
						Load_Password((CurrentPage*24+1),(PassSize-CurrentPage*6));
						Display_Password((PassSize-CurrentPage*6));
						CurrentPos=temp;
						Segment[0]=temp0;Segment[1]=temp1;Segment[2]=temp2;Segment[3]=temp3;
				}
				else if (testkey==0 && FirstPress!=-1){
					if (Delay_Device<=190){
							Delay_Device++;
					}				
				}
				close_seven_segment();
				show_seven_segment(0,Segment[3]);
				delay_loop();
				
				close_seven_segment();
				show_seven_segment(1,Segment[2]);
				delay_loop();
				
				close_seven_segment();
				show_seven_segment(2,Segment[1]);
				delay_loop();
				
				close_seven_segment();
				show_seven_segment(3,Segment[0]);
				delay_loop();
				close_seven_segment();
				//for (i=0;i<100000;i++);*/
	}
}


