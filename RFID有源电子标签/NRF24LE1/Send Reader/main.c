/*********************************************************************

����:
	RFID����д����д����

˵��:
	1.ͨ�������ϱ�����д����Ӷ�д��ʶ�𵽵ı�ǩ��Ϣ
	2.���ǩ�·�����/����ָ��,�л���ǩ����״̬
	3.���ǩ�·���Ƶ�ź�ǿ������ָ��,���ڱ�ǩ�źŷ������
	4.ͨ��SPI���ܴӶ�дģ��ʶ�𵽵ı�ǩID��Ϣ

//P0.4, RXD, set as input
//P0.3, TXD, set as output

**********************************************************************/

#include <Nordic\reg24le1.h>
#include <stdint.h>
#include "API.h"
#include <absacc.h>

#define	PIN32

#ifdef 	PIN32
sbit  spi_cs= P0^6;
sbit LED4 = P0^6;      
sbit  spi_cs_slave=P1^1;   

#define p0dir	0xb0
#define p1dir	0xf3
#endif

#define INTERRUPT_RFIRQ	9
#define TX_ADR_WIDTH    8   				// RF�շ���ַ��8 bytes 
#define TX_PLOAD_WIDTH  32  					// ���ݰ�����Ϊ9 bytes

uint8_t const TX_ADDRESS[TX_ADR_WIDTH]     = {0xD0,0xEC,0xC8,0xB5,0xC7,0xBF,0x51,0x51}; // ����RF�շ���ַ
uint8_t const RX_ADDRESS[TX_ADR_WIDTH]  = {0xD2,0xF8,0xD0,0xD0,0xBD,0xA3,0xBF,0xCD}; // ����RF�շ���ַ0
uint8_t const RX_ADDRESS_P1[TX_ADR_WIDTH]  = {0xb7,0xb2,0xb3,0xb4,0x01,0x11,0x11,0x11}; // ����RF�շ���ַ1
// 2A 01 02 03 08 AA 0F 03 00 00 00 23 59 13 14 15 16 FF 01 01 01 00 81 85 08 0F 20 22 00 00 00 00 00 7E
uint8_t xdata uartrx_buf[TX_PLOAD_WIDTH +2];

uint8_t xdata rx_buf[TX_PLOAD_WIDTH];

uint8_t xdata  tx_buf[TX_PLOAD_WIDTH]={0x99};


//uint8_t data data_buf[10]  = {0x00,0x01,0x00,0x01,0x00,0x04,0x03,0x02,0x01,0x00};	//д��NVN�洢���������

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
  	SPI_RW_Reg(WRITE_REG + CONFIG, 0x0e);     			// �ϵ�, CRCΪ2 bytes,����ģʽ,����RX_DR�����ж�
  	
	SPI_Write_Buf(WR_TX_PLOAD, tx_buf, TX_PLOAD_WIDTH); // д���ݵ�FIFO
	RFCE=1;												// ��������																				
}

/**************************************************
���ܣ�RF��ʼ��
**************************************************/
void rf_init(void)
{
  	RFCE = 0;                                   		// RF�ر�
  	RFCKEN = 1;                                 		// ����RFʱ��
  	RF = 1;                                     		// ����RF�ж�

	delay(200);

    SPI_Write_Buf(WRITE_REG + TX_ADDR, TX_ADDRESS, TX_ADR_WIDTH);    	// ���÷����ַ����
  	SPI_Write_Buf(WRITE_REG + RX_ADDR_P0, RX_ADDRESS, TX_ADR_WIDTH); 	// ���ý��յ�ַ0����
	  SPI_Write_Buf(WRITE_REG + RX_ADDR_P1, RX_ADDRESS_P1, TX_ADR_WIDTH); 	// ���ý��յ�ַ1����

	  SPI_RW_Reg(WRITE_REG + RX_PW_P0, TX_PLOAD_WIDTH); 	// PIPE0 �������ݰ�����	
    SPI_RW_Reg(WRITE_REG + RX_PW_P1, TX_PLOAD_WIDTH); 	// PIPE1 �������ݰ�����	

  	SPI_RW_Reg(WRITE_REG + EN_AA, 0x03);      			// �����Զ�Ӧ����
  	SPI_RW_Reg(WRITE_REG + EN_RXADDR, 0x03);  			// PIPE��������
  	SPI_RW_Reg(WRITE_REG + SETUP_RETR, 0x1a); 			// �Զ��ش�10��
  	SPI_RW_Reg(WRITE_REG + RF_CH, 40);        			// RFƵ��2440MHz
  	SPI_RW_Reg(WRITE_REG + RF_SETUP, 0x0f);   			// ���书��0dBm, ��������2Mbps,
  		
} 

/**************************************************
���ܣ�RF�жϷ������
**************************************************/
void RF_IRQ(void) interrupt INTERRUPT_RFIRQ
{
	sta=SPI_Read(STATUS);								// ����״ֵ̬

	if(RX_DR)									
	{
		SPI_Read_Buf(RD_RX_PLOAD,rx_buf,TX_PLOAD_WIDTH);// ����FIFO������
		SPI_RW_Reg(FLUSH_RX,0);							// ���RX��FIFO
	}

	SPI_RW_Reg(WRITE_REG+STATUS,0x70);					// ��������жϱ�־ 
}
/**************************************************
���ܣ����ڳ�ʼ���ӳ���
˵����������9600��ʹ���ڲ������ʷ�����

������      S0REL

600          0x00BF
1200         0X025F
2400         0X0330
4800         0X0398
9600         0X03CC
19200        0X03E6
38400        0X03F3

**************************************************/
void uart_init(void)
{
    ES0 = 0;                      				// ��UART0�ж�
    REN0 = 1;                     				// �������
    SM0 = 0;                      				// ����ģʽ1��8bit�ɱ䲨����
    SM1 = 1;                   
    PCON |= 0x80;                 				// SMOD = 1
	
    ADCON |= 0x80;                				// ѡ���ڲ������ʷ�����
    S0RELL = 0xF3;                				// ������38400
    S0RELH = 0x03;								
    TI0 = 0;					  				// �巢����ɱ�־
	  RI0=0;					  				   // �������ɱ�־
	  S0BUF=0x00;					  				// �ͳ�ֵ
}
/**************************************************
���ܣ��򴮿ڷ���1 byte����
**************************************************/
void uart_putchar(uint8_t x)
{
	while (!TI0);								// �ȴ��������
	TI0=0;										// �巢����ɱ�־
	S0BUF=x;									// ��������
}

///**************************************************
//���ܣ����ڽ���1 byte����
//**************************************************/
//uint8_t uart_receive_char(uint8_t x)
//{
//	while (!RI0);		// �ȴ��������
//	RI0=0;			// �巢����ɱ�־
//	x=S0BUF;		// ��������
//	return	x;
//}

/**************************************************
����:I/O�ڳ�ʼ��
**************************************************/
void io_init(void)
{
  	P0DIR = p0dir;							   	// �趨I/O���������
  	P1DIR = p1dir;					
	delay(100);               
}  


/**************************************************
���ܣ�������
**************************************************/
void main(void)
{
    uint8_t readerid=0x08;

	//���յ������·���ָ��:0xdd 0x**�л���ǩ����״̬�����ñ�ǩ��Ƶ�ź�ǿ�� 	
	uint8_t yy[2]={0xff,0xff} ; 	
	uint8_t  ii, j=0,  kk[2]={0,0}, tagpower[2]={0x0f,0x0f}, m=0, n=0 ,k = 0;
	ii=0;
		
	io_init();									// I/O�ڳ�ʼ��
	uart_init();                // ���ڳ�ʼ�� 
	rf_init();									// RF��ʼ��                            
  EA=1; 										// �����ж�                          

	RX_Mode();	   //�������ģʽ
	//SPI����	
	SPIMCON0 = 0xF1;  		  //  	6 	5 	4 	3 	2 	1 	0 
					 //	1	0	0	0	0	0	1
	SPISCON0 = 0xA1;  		  //  	7	6 	5 	4 	3 	2 	1 	0 
							  //	1	0	1	0	0	0	0	1
//	uart_putchar(0x2A);	
	
	

	while(1)
	{
		
	//		RX_Mode();	   //�������ģʽ
		// 2A    01 02 03 08   AA    0F     03 00 00 00 23 59 13 14 15 16 FF 01 01 01 00 81 85 08 0F 20 22 00 00 00 00 00 7E
		 //       ����              �ź�ǿ��
		
		/***********************���ڽ��ո���ָ��**********************/
		if (RI0 )
		{
		   RI0=0;			// �巢����ɱ�־
		   uartrx_buf[ii]=S0BUF;
			
				if (0X2A == uartrx_buf[0])
				{	
		      ii++;
										
				}
				else
				{
           ii =0;
				}	
		   if (ii>=34) 
			 {
			 		 ii=0;
				 
					if((uartrx_buf[33] == 0x7E))
					{
						ii =0;

						     for(k = 0 ; k< 32 ; k ++)
						       {
									  tx_buf[k] = uartrx_buf[k+1];
									 }
						
				TX_Mode();								// ��������
	       while (!(TX_DS|MAX_RT));				// �ȴ�������� 				 
				sta = 0 ;					 
									 
//					uart_putchar(tx_buf[0]);	
//					uart_putchar(tx_buf[1]);
//					uart_putchar(tx_buf[2]);
//					uart_putchar(tx_buf[3]);
//					uart_putchar(tx_buf[4]);
//					uart_putchar(tx_buf[5]);
//					uart_putchar(tx_buf[6]);
//					uart_putchar(tx_buf[7]);
//					uart_putchar(tx_buf[8]);
//					uart_putchar(tx_buf[9]);
//					uart_putchar(tx_buf[10]);					
//					uart_putchar(tx_buf[11]);
//					uart_putchar(tx_buf[12]);
//					uart_putchar(tx_buf[13]);
//					uart_putchar(tx_buf[14]);	
//					uart_putchar(tx_buf[15]);
//					uart_putchar(tx_buf[16]);
//					uart_putchar(tx_buf[17]);
//					uart_putchar(tx_buf[18]);
//					uart_putchar(tx_buf[19]);
//					uart_putchar(tx_buf[20]);
//					uart_putchar(tx_buf[21]);
//					uart_putchar(tx_buf[22]);
//					uart_putchar(tx_buf[23]);
//					uart_putchar(tx_buf[24]);					
//					uart_putchar(tx_buf[25]);
//					uart_putchar(tx_buf[26]);
//					uart_putchar(tx_buf[27]);					
//					uart_putchar(tx_buf[28]);
//					uart_putchar(tx_buf[29]);
//					uart_putchar(tx_buf[30]);
//					uart_putchar(tx_buf[31]);									 
																 								 
					}
			 }
			 
			 
			 RX_Mode();	   //�������ģʽ
		}


		/***********0xdd 0x11/0x22 : ���ջ���/����״̬���ָ����·�״̬���ָ���TAG**********/
		/**0xdd 0x09(0dBm)/0x0b(-6dBm)/0x0d(-12dBm)/0x0f(-18dBm) :������Ƶ���书������ָ����·���Ƶ�������ø���ǩ**/
//		if(yy[0]==0xdd)
//		{
//		   	if(yy[1]!=0)
//	        {
//	           tx_buf[0]=yy[1];
//			   if(tx_buf[0]!=0xdd)
//			   {
//	             for(j=0;j<180;j++)
//	             {
//	               TX_Mode();								// ��������
//	               while (!(TX_DS|MAX_RT));				// �ȴ��������
//	               sta = 0;
//	             }
//	             RX_Mode();
//	            }
//			 } 
//		} 

          

				if(RX_DR)								// �������յ�
				{
					sta=0;
					/***********��֡�ϴ�������************/
					uart_putchar(0x2A);				
					uart_putchar(0x20);							
					uart_putchar(rx_buf[0]);	
					uart_putchar(rx_buf[1]);
					uart_putchar(rx_buf[2]);
					uart_putchar(rx_buf[3]);
					uart_putchar(rx_buf[4]);
					uart_putchar(rx_buf[5]);
					uart_putchar(rx_buf[6]);
					uart_putchar(rx_buf[7]);
					uart_putchar(rx_buf[8]);
					uart_putchar(rx_buf[9]);
					uart_putchar(rx_buf[10]);					
					uart_putchar(rx_buf[11]);
					uart_putchar(rx_buf[12]);
					uart_putchar(rx_buf[13]);
					uart_putchar(rx_buf[14]);	
					uart_putchar(rx_buf[15]);
					uart_putchar(rx_buf[16]);
					uart_putchar(rx_buf[17]);
					uart_putchar(rx_buf[18]);
					uart_putchar(rx_buf[19]);
					uart_putchar(rx_buf[20]);
					uart_putchar(rx_buf[21]);
					uart_putchar(rx_buf[22]);
					uart_putchar(rx_buf[23]);
					uart_putchar(rx_buf[24]);					
					uart_putchar(rx_buf[25]);
					uart_putchar(rx_buf[26]);
					uart_putchar(rx_buf[27]);					
					uart_putchar(rx_buf[28]);
					uart_putchar(rx_buf[29]);
					uart_putchar(rx_buf[30]);
					uart_putchar(rx_buf[31]);
					uart_putchar(0x7E);	
					uart_putchar(0x7E);	
					uart_putchar(0x7E);	
					
//					uart_putchar(rx_buf[32]);											
				}	
		}
				
		//uart_putchar(0x10);		
	}	
             				