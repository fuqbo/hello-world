/*********************************************************************

����:
	��ǩTAG���г���

˵��:
    1.��ǩ��������������Ϣ��ID��� + ��Ƶ�����ź�ǿ�ȣ�
	2.��ǩ������״̬���л�:   
	                       ״̬1�������������ͣ�����-����-���ͣ�
					       ״̬2���������գ�����-���գ�

**********************************************************************/


#include <Nordic\reg24le1.h>
#include <stdint.h>
#include "API.h"
#include <absacc.h>

#define	PIN32
#ifdef 	PIN32
#define p0dir	0xF0
#define p1dir	0xF7
#endif

#define LED0 P13

#define	ID_MEMORY_BASE	0xFC00
#define	ID_LENGTH	       9        //ǰ8���ֽ�Ϊ���кţ���9���ֽڹ̶�Ϊ0XA5��ʾ��д��ID

#define eddr            0xFA00	  //�趨��Ƶ����ֵ����NVM�洢����ַ

#define SEND_CYCLE      2  //���ʹ���


#define INTERRUPT_RFIRQ	9
#define INTERRUPT_TICK 	13  

#define TX_ADR_WIDTH    8   			    // RF�շ���ַ��8 bytes 
#define TX_PLOAD_WIDTH  32  					// ���ݰ�����Ϊ32 bytes
#define RX_PLOAD_WIDTH  32  					// ���ݰ�����Ϊ9 bytes

uint8_t const TX_ADDRESS[TX_ADR_WIDTH]  = {0xD2,0xF8,0xD0,0xD0,0xBD,0xA3,0xBF,0xCD}; //����RF���͵�ַ
uint8_t const RX_ADDRESS[TX_ADR_WIDTH]  = {0xD0,0xEC,0xC8,0xB5,0xC7,0xBF,0x51,0x51};			 //����RF���յ�ַ

uint8_t	data uid=0x03;      //����ǩ��ID��(�Լ���¼)
uint8_t data txpower=0x0f;		//��ǩ��Ƶ���书��
uint8_t data rx_buf[RX_PLOAD_WIDTH];
uint8_t data tx_buf[TX_PLOAD_WIDTH]={0x00, 0x00, 0x11, 0x22, 0x33, 0x66, 0x03, 0x00, 0x00, 0x00, 
                                     0x23, 0x59, 0x13, 0x14, 0x15, 0x16, 0xFF, 0x01, 0x01, 0x01, 
                                     0x00, 0x81, 0x85, 0x08, 0x0F, 0x20, 0x22, 0x00, 0x00, 0x00,
                                     0x00, 0x00};

/*

�ź�ǿ��   1�ֽ�
����       1�ֽ�
����       4�ֽ�																		 
ʱ���ģʽ 1�ֽ�
ǰ����     1�ֽ�
ÿ��ʱ��   4�ֽ�
��������   4�ֽ�
����       1�ֽ�
¥��Ȩ��   12�ֽ�
����       3�ֽ�
*/																		 
																		 
																		 
uint8_t bdata sta;
sbit	RX_DR	=sta^6;
sbit	TX_DS	=sta^5;
sbit	MAX_RT	=sta^4;

/**************************************************
���ܣ���ʱ
**************************************************/
void delay(uint16_t x)
{
    uint16_t i,j;
    i=0;
    for(i=0;i<x;i++)
    {
       j=108;
           ;
       while(j--);
    }
}
/**************************************************
���ܣ�Ӳ��SPI��д
**************************************************/
uint8_t SPI_RW(uint8_t value)
{
  SPIRDAT = value;
  											       
  while(!(SPIRSTAT & 0x02));  					// �ȴ�SPI�������

  return SPIRDAT;             					// ���ض���ֵ
}
/**************************************************
���ܣ�дRF�Ĵ�������RF״ֵ̬
**************************************************/
uint8_t SPI_RW_Reg(uint8_t reg, uint8_t value)
{
	uint8_t status;

  	RFCSN = 0;                   	
  	status = SPI_RW(reg);      					// ѡ��RF�Ĵ���
  	SPI_RW(value);             					// д������
  	RFCSN = 1;                   	

  	return(status);            					// ����RF״ֵ̬
}
/**************************************************
���ܣ���RF�Ĵ���
**************************************************/
uint8_t SPI_Read(uint8_t reg)
{
	uint8_t reg_val;

  	RFCSN = 0;                			
  	SPI_RW(reg);            					// ѡ��RF�Ĵ���
  	reg_val = SPI_RW(0);    					// ��������
  	RFCSN = 1;                			

  	return(reg_val);        					// ����RF״ֵ̬
}

/**************************************************
���ܣ���RF�Ĵ������ֽ����ݵ�������
**************************************************/
uint8_t SPI_Read_Buf(uint8_t reg, uint8_t *pBuf, uint8_t bytes)
{
	uint8_t status,byte_ctr;

  	RFCSN = 0;                    		
  	status = SPI_RW(reg);       				// ѡ��RF�Ĵ���

  	for(byte_ctr=0;byte_ctr<bytes;byte_ctr++)
    	pBuf[byte_ctr] = SPI_RW(0);    			// ���Ӷ�������

  	RFCSN = 1;                          

  	return(status);                    			// ����RF״ֵ̬
}

/**************************************************
���ܣ��ѻ������Ķ��ֽ�����д��RF�Ĵ���
**************************************************/
uint8_t SPI_Write_Buf(uint8_t reg, uint8_t *pBuf, uint8_t bytes)
{
	uint8_t status,byte_ctr;

  	RFCSN = 0;                   		
  	status = SPI_RW(reg);    					// ѡ��RF�Ĵ���
  	for(byte_ctr=0; byte_ctr<bytes; byte_ctr++) // ����д������
    	SPI_RW(*pBuf++);
  	RFCSN = 1;                 			
  	return(status);          					// ����RF״ֵ̬
}
/**************************************************
���ܣ�����Ϊ����ģʽ
**************************************************/
void PD_Mode(void)
{
  	RFCE=0;
  	SPI_RW_Reg(WRITE_REG + CONFIG, 0x0c);   	// PWR_UP=0
}

/**************************************************
���ܣ�����Ϊ����ģʽ
**************************************************/
void RX_Mode(void)
{
	  RFCE=0;
  	SPI_RW_Reg(WRITE_REG + CONFIG, 0x0f);   	// �ϵ�, CRCΪ2 bytes,����ģʽ,����RX_DR�����ж�
  	RFCE = 1; 									// ��������ģʽ
}

/**************************************************
���ܣ�����Ϊ����ģʽ
**************************************************/
void TX_Mode(void)
{
	RFCE=0;
	SPI_RW_Reg(FLUSH_TX,0);								// ���TX��FIFO
  	SPI_RW_Reg(WRITE_REG + CONFIG, 0x0e);     			// �ϵ�, CRCΪ2 bytes,����ģʽ,����RX_DR�����ж�  	
	SPI_Write_Buf(WR_TX_PLOAD, tx_buf, TX_PLOAD_WIDTH); // д���ݵ�FIFO
	RFCE=1;	
}
/**************************************************
���ܣ�RF��ʼ��
**************************************************/
void rf_init(void)
{
  	RFCE = 0;                                   		// RF�ر�
  	RFCKEN = 1;                                 		// ����RFʱ��
  	RF = 1;                                     		// ����RF�ж�
	delay(1000);

  	SPI_Write_Buf(WRITE_REG + TX_ADDR, TX_ADDRESS, TX_ADR_WIDTH);    	// ���÷����ַ����	
	SPI_Write_Buf(WRITE_REG + RX_ADDR_P0, RX_ADDRESS, TX_ADR_WIDTH); 	// ���ý��յ�ַ���� 

  	SPI_RW_Reg(WRITE_REG + EN_AA, 0x01);      			// �����Զ�Ӧ����
  	SPI_RW_Reg(WRITE_REG + EN_RXADDR, 0x01);  			// PIPE0��������
  	SPI_RW_Reg(WRITE_REG + SETUP_RETR, 0x1a); 			// �Զ��ش�10��
  	SPI_RW_Reg(WRITE_REG + RF_CH, 40);        			// RFƵ��2440MHz
  	SPI_RW_Reg(WRITE_REG + RF_SETUP, txpower);   	    // ���÷��书��txpower, ��������2Mbps,
  	SPI_RW_Reg(WRITE_REG + RX_PW_P0, TX_PLOAD_WIDTH); 	// PIPE0 �������ݰ�����		*/	
} 

/**************************************************
���ܣ�RF�жϷ������
**************************************************/
void RF_IRQ(void) interrupt INTERRUPT_RFIRQ
{
	sta=SPI_Read(STATUS);								// ����״ֵ̬
	
	/***************************��ǩ״̬�л���Ӧ�����***********************/
	if(RX_DR)									
	{
		SPI_Read_Buf(RD_RX_PLOAD,rx_buf,TX_PLOAD_WIDTH);// ����FIFO������
		SPI_RW_Reg(FLUSH_RX,0);							// ���RX��FIFO
	}
	/*************************************************************************/

	SPI_RW_Reg(WRITE_REG+STATUS,0x70);					// ��������жϱ�־ 
}

/**************************************************
���ܣ��������������ʼ���ӳ���
**************************************************/
//void rng_init(void)
//{
//   RNGCTL=0x80;               //���������������
//}

/*���õ�ѹ�����Ĵ���*/
void PWR_CONFIG()
{
POFCON|=0X80;//ʹ�ܹ���ʧ�ܱ��������ñȽϵ�ѹΪ2.1v
}

/*��ȡ��Դʧ�ܵ�ԭ��*/
unsigned char RD_POF()
{
return (POFCON&=0X10) ;	//�����0���Ǹߣ�����ȥ1���ǵ�ѹ���ڱȽ�ֵ
}

//��ȡд��ȥ��ID
void read_id_from_nvdata(void)
{


	unsigned char id[ID_LENGTH];
	unsigned char i;
	for(i=0;i<ID_LENGTH;i++)
	{
		id[i] = XBYTE[ID_MEMORY_BASE+i];  
		
	//	hal_uart_putchar(id[i]);
	}
	if(id[8]!=0xA5)
	{
		//оƬûд�����кţ�ʹ��Ĭ�����кŻ򱨴�
		id[0]  =  0x01;
		id[1]  =  0x02;
		id[2]  =  0x03;
		id[3]  =  0x04;
		id[4]  =  0x05;
		id[5]  =  0x06;
		id[6]  =  0x07;
		id[7]  =  0x08;			
	}
	
  tx_buf[2]  	=	id[3] ;
	tx_buf[3]   = id[2] ;
	tx_buf[4]   = id[1] ;
	tx_buf[5]	  = id[0] ;
 	
	
}

/**************************************************
���ܣ�RTC2��ʼ��
**************************************************/
void rtc2_init(void)
{
	CLKLFCTRL=0x01;									   	// ʹ��RC 32KHzʱ��
	
	RTC2CMP0=0xFF;										// ��ʱ2��
	if(SEND_CYCLE <=5)
	{
		RTC2CMP1=0x20;        //0X80--1S
	
	}
	else
	{
		RTC2CMP1=0xFF;        //0X80--1S
	}

	
	RTC2CON=0x06;										// �Ƚ�ģʽ
	WUIRQ=1;											// ����TICK�ж�
}
/**************************************************
���ܣ�����RTC2
**************************************************/
void rtc2_on(void)
{
    //RTC2��ʱ�������
	RTC2CON |=0x01;									
}
/**************************************************
���ܣ��ر�RTC2
**************************************************/
void rtc2_off(void)
{													  	
	RTC2CON &=~0x01;							   		
}
/**************************************************
���ܣ�RTC2�жϷ������
**************************************************/
void RTC2_IRQ(void) interrupt INTERRUPT_TICK 
{
	//LED2=!LED2;										   	
}              				
/**************************************************
����:I/O�ڳ�ʼ��
**************************************************/
void io_init(void)
{
  	P0DIR = p0dir;							   	// �趨I/O���������
  	P1DIR = p1dir;					                
}  

/**************************************************
���ܣ����ڳ�ʼ���ӳ���	 *****������
˵����������9600��ʹ���ڲ������ʷ�����
**************************************************/
//void uart_init(void)
//{
//    ES0 = 0;                      				// ��UART0�ж�
//    REN0 = 1;                     				// �������
//    SM0 = 0;                      				// ����ģʽ1��8bit�ɱ䲨����
//    SM1 = 1;                   
//    PCON |= 0x80;                 				// SMOD = 1
//	
//    ADCON |= 0x80;                				// ѡ���ڲ������ʷ�����
//    S0RELL = 0xcc;                				// ������9600(ʮ����972=ʮ������0x03cc
//    S0RELH = 0x03;							
//    TI0 = 0;					  				// �巢����ɱ�־
//	RI0=0;					  				   // �������ɱ�־
//	S0BUF=0x99;					  				// �ͳ�ֵ
//}
/**************************************************
���ܣ��򴮿ڷ���1 byte����	*****������
**************************************************/
//void uart_putchar(uint8_t x)
//{
//	while (!TI0);								// �ȴ��������
//	TI0=0;										// �巢����ɱ�־
//	S0BUF=x;									// ��������
//}

/**************************************************
���ܣ�NVM�洢�����ֽڲ���/д���ӳ���
˵�����Ȳ�������ҳ��Ȼ��д������
**************************************************/
void hal_flash_bytes_write(uint16_t a,uint8_t *p,uint16_t n)
{
   uint8_t xdata * data pb;
   if((a>=0xfa00)&&(a<=0xffff))	   //NVM��ַ��
   {
     F0=EA;
	 EA=0; 
	 WEN=1;             //����д��
	 if((a>=0xfa00)&&(a<0xfc00))   //��ַ��NVM�ӳ�������
	 {
	    FCR=32;	               //�����������ҳ��512�ֽڣ�
		while(RDYN==1);
		FCR=33;
		while(RDYN==1);
	 }
	 if((a>=0xfc00)&&(a<=0xffff))      //��ַ��NVM��ͨ������
	 {
	   FCR=34;                   //������ҳ  
	   while(RDYN==1);
	 }
	 if((a>=0xfe00)&&(a<=0xffff))      //��ַ��NVM��ͨ������
	 {
	   FCR=35;                   //������ҳ  
	   while(RDYN==1);
	 }
	 delay(1000);
	
	 pb=(uint8_t xdata *)a;
	 while(n--)				  //����д������
	 {
	   *pb++=*p++;
	   while(RDYN==1);
	 }
	 WEN=0;					  //д�꣬��ֹд
	 EA=F0;				      //�ָ��ж�
   }
}



//���÷��书��+Ψһ��־  1�ֽ�+3�ֽ�
void set_rf_txpower (void)
	{
		if((rx_buf[5]==0x0f)||(rx_buf[5]==0x0d)||(rx_buf[5]==0x0b)||(rx_buf[5]==0x09))								
		  {	
//				txpower = rx_buf[5]; 
//				
//				rf_init();	
		
				hal_flash_bytes_write(eddr,rx_buf +4,28);								
		  }
	}


/****************
/
/nRF24LE1�ĵ͹���ģʽ����
/
****************/
void  Low_Power_IO_Set(void)
{

	P0CON = 0x70;  //P0.0����Ϊ�������뻺���ģʽ
	P0CON = 0x71;  //P0.1����Ϊ�������뻺���ģʽ
	P0CON = 0x72;  //P0.2����Ϊ�������뻺���ģʽ
	P0CON = 0x73;  //P0.3����Ϊ�������뻺���ģʽ
	P0CON = 0x74;  //P0.4����Ϊ�������뻺���ģʽ
	P0CON = 0x75;  //P0.5����Ϊ�������뻺���ģʽ
	P0CON = 0x76;  //P0.6����Ϊ�������뻺���ģʽ
	P0CON = 0x77;  //P0.7����Ϊ�������뻺���ģʽ
	P0DIR = 0xFF;  //P0.0~P0.7��������Ϊ����
	
	
	P1CON = 0x70;  //P1.0����Ϊ�������뻺���ģʽ
	P1CON = 0x71;  //P1.1����Ϊ�������뻺���ģʽ
	P1CON = 0x72;  //P1.2����Ϊ�������뻺���ģʽ
	P1CON = 0x73;  //P1.3����Ϊ�������뻺���ģʽ
	P1CON = 0x74;  //P1.4����Ϊ�������뻺���ģʽ
	P1CON = 0x75;  //P1.5����Ϊ�������뻺���ģʽ
	P1CON = 0x76;  //P1.6����Ϊ�������뻺���ģʽ

	P1DIR = 0x7F;  //P1.0~P1.6��������Ϊ����

}
	

/**************************************************
���ܣ�������
**************************************************/
void main(void)
{	
	
	uint16_t  cycle = 0 ,jj = 0;
	
  uint8_t   i = 0, j=0 , k=0 ,kk = 0;

	io_init();									// I/O�ڳ�ʼ��

	LED0=1;            //���
	
//	P1DIR = 0XFF;		   //���ó�����
	
  PWR_CONFIG();  	/*���õ�ѹ�����Ĵ���*/
	
  read_id_from_nvdata();  	//��ȡд��ȥ��ID
	
	//���ó�ʼ���书��
	if(XBYTE[eddr+0]==0xAA)   
	{
	 txpower=XBYTE[eddr+1];	
		
		for( i = 0 ; i < 26 ; i ++)
		{
		  tx_buf[i + 6 ]   =  XBYTE[eddr + 2 + i];
		}
	}		
	else 
	{

		
	 //    txpower=XBYTE[eddr+0]; 
		
		for( i = 0 ; i < 26 ; i ++)
			{
				tx_buf[i + 6 ]   =  0;
			}	
	}  
	
	tx_buf[0] =  txpower ;     //�ź�ǿ��
	
	rf_init();									// RF��ʼ��                            
  EA=1;                       // �����ж�	 
	rtc2_init();                // RTC2��ʼ��
			
	if(RD_POF())					 //��ⷴ����ǰ�Ĺ���ĵ�ѹ���  
		{	
		tx_buf[1]  = 0x00 ;     //��ѹ
		}
  else
	 {
		tx_buf[1]  = 0x80 ;
	 }
	 
	 RX_Mode(); 	  	//�������ģʽ
	 
	while(1)
	{

			rtc2_off();								// ��RTC2	
		
		if(jj >1500)
		{
					
			
			
			
		/**********************״̬1�������������ͣ�����-����-���ͣ�************************/
 
		   sta=0;	
			 delay(10);
		 
			TX_Mode();								// ���䱾TAG��ԴID����				
		     while (!(TX_DS|MAX_RT));				// �ȴ��������			
			sta = 0; 				
			
			PD_Mode();								// ��RF
							
			if(!kk)	{	  kk ++;	}		
				
			tx_buf[1]  |= kk ;     //	
								
			if(cycle>=SEND_CYCLE)    //2s/�� 150 5����    
				{ 
				
					Low_Power_IO_Set();
					
				  OPMCON |= 0x02;          //I/O״̬����
					
          PWRDWN = 0x01;           //�������˯��ģʽ�����ѻᵼ�¸�λ
				}	
  
       else
			 {			
            cycle ++ ;
				 
			 		  rtc2_on();								// ��RTC2			 
				 				 
				 	  Low_Power_IO_Set();
				 
				 		PWRDWN = 0x04;							// ����͹���ģʽ3.2uA���Ĵ���ά�֣�clklf����RCOSC32K���ȴ�RTC2����				 
			 }  
		}
		else
		{
			
			jj ++;
			delay(1);
		
			if(RX_DR)								// �������յ�
				{					
					sta = 0;
					
				if(((rx_buf[0]==tx_buf[2])&&(rx_buf[1]==tx_buf[3])&&(rx_buf[2]==tx_buf[4])&&(rx_buf[3]==tx_buf[5]))||((rx_buf[0]==0xFF)&&(rx_buf[1]==0xFF)&&(rx_buf[2]==0xFF)&&(rx_buf[3]==0xFF)))				
		      {	
											
						switch (rx_buf[4]) //��������
							{
								
										case 0xAA: set_rf_txpower();                     break;     //���÷��书��
		//								case 0x02: GetVersion();                      break;     //��ȡ�汾��Ϣ
		//								case 0x03: SetMachno();                       break;     //���û���
		//								case 0x04: GetMachno();                       break;     //��ȡ����		
		//								case 0x05: SetTime();                         break;     //����ʱ��
								
										default:  break;
							}				
					}	
					
				}	
		
		}
	
		  /*************************** ״̬2���������գ�����-���գ�******************************/
//		  if(rx_buf[0]==0x22)								// �������յ�
//		  {	
//			sta=0;
//			delay(15);
//			PD_Mode();								// ��RF
//		    rtc2_on();								// ��RTC2
//		    PWRDWN = 0x04;							// ����͹���ģʽ���ȴ�RTC2����
//		    RX_Mode(); 
//			memrx=0x22;	
//				
//				
//				
//		  }

		  /*****************������Ƶ��������ָ��,���ñ�ǩ���书��,��д��NVM�洢��****************/
		  /******************  0x0f:0dBm 
		                       0x0d:-6dBm
							   0x0b:-12dBm
							   0x09:-18dBm  *****************/
//		  if((rx_buf[0]==0x0f)||(rx_buf[0]==0x0d)||(rx_buf[0]==0x0b)||(rx_buf[0]==0x09))								
//		  {	
//			sta=0;
//			txpower=rx_buf[0]; 
//			rf_init();								                        	
//			hal_flash_bytes_write(eddr,&txpower,1);
//			rx_buf[0]=memrx;
//		  }
//		}
	}
}              				


