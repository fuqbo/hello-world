/*********************************************************************

功能:
	RFID主读写器读写程序

说明:
	1.通过串口上报主读写器与从读写器识别到的标签信息
	2.向标签下发休眠/唤醒指令,切换标签工作状态
	3.向标签下发射频信号强度配置指令,调节标签信号发射距离
	4.通过SPI接受从读写模块识别到的标签ID信息

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
#define TX_ADR_WIDTH    8   				// RF收发地址共8 bytes 
#define TX_PLOAD_WIDTH  32  					// 数据包长度为9 bytes

uint8_t const TX_ADDRESS[TX_ADR_WIDTH]     = {0xD0,0xEC,0xC8,0xB5,0xC7,0xBF,0x51,0x51}; // 定义RF收发地址
uint8_t const RX_ADDRESS[TX_ADR_WIDTH]  = {0xD2,0xF8,0xD0,0xD0,0xBD,0xA3,0xBF,0xCD}; // 定义RF收发地址0
uint8_t const RX_ADDRESS_P1[TX_ADR_WIDTH]  = {0xb7,0xb2,0xb3,0xb4,0x01,0x11,0x11,0x11}; // 定义RF收发地址1
// 2A 01 02 03 08 AA 0F 03 00 00 00 23 59 13 14 15 16 FF 01 01 01 00 81 85 08 0F 20 22 00 00 00 00 00 7E
uint8_t xdata uartrx_buf[TX_PLOAD_WIDTH +2];

uint8_t xdata rx_buf[TX_PLOAD_WIDTH];

uint8_t xdata  tx_buf[TX_PLOAD_WIDTH]={0x99};


//uint8_t data data_buf[10]  = {0x00,0x01,0x00,0x01,0x00,0x04,0x03,0x02,0x01,0x00};	//写入NVN存储区域的数据

uint8_t bdata sta;
sbit	RX_DR	=sta^6;
sbit	TX_DS	=sta^5;
sbit	MAX_RT	=sta^4;

/**************************************************
功能：延时
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
功能：硬件SPI读写
**************************************************/
uint8_t SPI_RW(uint8_t value)
{
  SPIRDAT = value;
  											       
  while(!(SPIRSTAT & 0x02));  					// 等待SPI传输完成

  return SPIRDAT;             					// 返回读出值
}
/**************************************************
功能：写RF寄存器，读RF状态值
**************************************************/
uint8_t SPI_RW_Reg(uint8_t reg, uint8_t value)
{
	uint8_t status;

  	RFCSN = 0;                   	
  	status = SPI_RW(reg);      					// 选择RF寄存器
  	SPI_RW(value);             					// 写入数据
  	RFCSN = 1;                   	

  	return(status);            					// 返回RF状态值
}
/**************************************************
功能：读RF寄存器
**************************************************/
uint8_t SPI_Read(uint8_t reg)
{
	uint8_t reg_val;

  	RFCSN = 0;                			
  	SPI_RW(reg);            					// 选择RF寄存器
  	reg_val = SPI_RW(0);    					// 读出数据
  	RFCSN = 1;                			

  	return(reg_val);        					// 返回RF状态值
}
/**************************************************
功能：读RF寄存器多字节数据到缓冲区
**************************************************/
uint8_t SPI_Read_Buf(uint8_t reg, uint8_t *pBuf, uint8_t bytes)
{
	uint8_t status,byte_ctr;

  	RFCSN = 0;                    		
  	status = SPI_RW(reg);       				// 选择RF寄存器

  	for(byte_ctr=0;byte_ctr<bytes;byte_ctr++)
    	pBuf[byte_ctr] = SPI_RW(0);    			// 连接读出数据

  	RFCSN = 1;                          

  	return(status);                    			// 返回RF状态值
}
/**************************************************
功能：把缓冲区的多字节数据写到RF寄存器
**************************************************/
uint8_t SPI_Write_Buf(uint8_t reg, uint8_t *pBuf, uint8_t bytes)
{
	uint8_t status,byte_ctr;

  	RFCSN = 0;                   		
  	status = SPI_RW(reg);    					// 选择RF寄存器
  	for(byte_ctr=0; byte_ctr<bytes; byte_ctr++) // 连接写入数据
    	SPI_RW(*pBuf++);
  	RFCSN = 1;                 			
  	return(status);          					// 返回RF状态值
}

/**************************************************
功能：设置为接收模式
**************************************************/
void RX_Mode(void)
{
	RFCE=0;
  	SPI_RW_Reg(WRITE_REG + CONFIG, 0x0f);   	// 上电, CRC为2 bytes,接收模式,允许RX_DR产生中断
  	RFCE = 1; 									// 启动接收模式
}

/**************************************************
功能：设置为发射模式
**************************************************/
void TX_Mode(void)
{
    RFCE=0;
  	SPI_RW_Reg(WRITE_REG + CONFIG, 0x0e);     			// 上电, CRC为2 bytes,接收模式,允许RX_DR产生中断
  	
	SPI_Write_Buf(WR_TX_PLOAD, tx_buf, TX_PLOAD_WIDTH); // 写数据到FIFO
	RFCE=1;												// 启动发射																				
}

/**************************************************
功能：RF初始化
**************************************************/
void rf_init(void)
{
  	RFCE = 0;                                   		// RF关闭
  	RFCKEN = 1;                                 		// 启动RF时钟
  	RF = 1;                                     		// 允许RF中断

	delay(200);

    SPI_Write_Buf(WRITE_REG + TX_ADDR, TX_ADDRESS, TX_ADR_WIDTH);    	// 设置发射地址长度
  	SPI_Write_Buf(WRITE_REG + RX_ADDR_P0, RX_ADDRESS, TX_ADR_WIDTH); 	// 设置接收地址0长度
	  SPI_Write_Buf(WRITE_REG + RX_ADDR_P1, RX_ADDRESS_P1, TX_ADR_WIDTH); 	// 设置接收地址1长度

	  SPI_RW_Reg(WRITE_REG + RX_PW_P0, TX_PLOAD_WIDTH); 	// PIPE0 接收数据包长度	
    SPI_RW_Reg(WRITE_REG + RX_PW_P1, TX_PLOAD_WIDTH); 	// PIPE1 接收数据包长度	

  	SPI_RW_Reg(WRITE_REG + EN_AA, 0x03);      			// 启动自动应答功能
  	SPI_RW_Reg(WRITE_REG + EN_RXADDR, 0x03);  			// PIPE接收数据
  	SPI_RW_Reg(WRITE_REG + SETUP_RETR, 0x1a); 			// 自动重传10次
  	SPI_RW_Reg(WRITE_REG + RF_CH, 40);        			// RF频率2440MHz
  	SPI_RW_Reg(WRITE_REG + RF_SETUP, 0x0f);   			// 发射功率0dBm, 传输速率2Mbps,
  		
} 

/**************************************************
功能：RF中断服务程序
**************************************************/
void RF_IRQ(void) interrupt INTERRUPT_RFIRQ
{
	sta=SPI_Read(STATUS);								// 读出状态值

	if(RX_DR)									
	{
		SPI_Read_Buf(RD_RX_PLOAD,rx_buf,TX_PLOAD_WIDTH);// 读出FIFO的数据
		SPI_RW_Reg(FLUSH_RX,0);							// 清除RX的FIFO
	}

	SPI_RW_Reg(WRITE_REG+STATUS,0x70);					// 清除所有中断标志 
}
/**************************************************
功能：串口初始化子程序
说明：波特率9600，使用内部波特率发生器

波特率      S0REL

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
    ES0 = 0;                      				// 关UART0中断
    REN0 = 1;                     				// 允许接收
    SM0 = 0;                      				// 串口模式1，8bit可变波特率
    SM1 = 1;                   
    PCON |= 0x80;                 				// SMOD = 1
	
    ADCON |= 0x80;                				// 选择内部波特率发生器
    S0RELL = 0xF3;                				// 波特率38400
    S0RELH = 0x03;								
    TI0 = 0;					  				// 清发送完成标志
	  RI0=0;					  				   // 清接收完成标志
	  S0BUF=0x00;					  				// 送初值
}
/**************************************************
功能：向串口发送1 byte数据
**************************************************/
void uart_putchar(uint8_t x)
{
	while (!TI0);								// 等待发送完成
	TI0=0;										// 清发送完成标志
	S0BUF=x;									// 发送数据
}

///**************************************************
//功能：串口接收1 byte数据
//**************************************************/
//uint8_t uart_receive_char(uint8_t x)
//{
//	while (!RI0);		// 等待发送完成
//	RI0=0;			// 清发送完成标志
//	x=S0BUF;		// 发送数据
//	return	x;
//}

/**************************************************
功能:I/O口初始化
**************************************************/
void io_init(void)
{
  	P0DIR = p0dir;							   	// 设定I/O口输入输出
  	P1DIR = p1dir;					
	delay(100);               
}  


/**************************************************
功能：主程序
**************************************************/
void main(void)
{
    uint8_t readerid=0x08;

	//接收到串口下发的指令:0xdd 0x**切换标签工作状态或配置标签射频信号强度 	
	uint8_t yy[2]={0xff,0xff} ; 	
	uint8_t  ii, j=0,  kk[2]={0,0}, tagpower[2]={0x0f,0x0f}, m=0, n=0 ,k = 0;
	ii=0;
		
	io_init();									// I/O口初始化
	uart_init();                // 串口初始化 
	rf_init();									// RF初始化                            
  EA=1; 										// 允许中断                          

	RX_Mode();	   //进入接收模式
	//SPI配置	
	SPIMCON0 = 0xF1;  		  //  	6 	5 	4 	3 	2 	1 	0 
					 //	1	0	0	0	0	0	1
	SPISCON0 = 0xA1;  		  //  	7	6 	5 	4 	3 	2 	1 	0 
							  //	1	0	1	0	0	0	0	1
//	uart_putchar(0x2A);	
	
	

	while(1)
	{
		
	//		RX_Mode();	   //进入接收模式
		// 2A    01 02 03 08   AA    0F     03 00 00 00 23 59 13 14 15 16 FF 01 01 01 00 81 85 08 0F 20 22 00 00 00 00 00 7E
		 //       卡号              信号强度
		
		/***********************串口接收各类指令**********************/
		if (RI0 )
		{
		   RI0=0;			// 清发送完成标志
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
						
				TX_Mode();								// 发射数据
	       while (!(TX_DS|MAX_RT));				// 等待发射结束 				 
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
			 
			 
			 RX_Mode();	   //进入接收模式
		}


		/***********0xdd 0x11/0x22 : 接收唤醒/休眠状态变更指令，并下发状态变更指令给TAG**********/
		/**0xdd 0x09(0dBm)/0x0b(-6dBm)/0x0d(-12dBm)/0x0f(-18dBm) :接收射频发射功率配置指令，并下发射频功率配置给标签**/
//		if(yy[0]==0xdd)
//		{
//		   	if(yy[1]!=0)
//	        {
//	           tx_buf[0]=yy[1];
//			   if(tx_buf[0]!=0xdd)
//			   {
//	             for(j=0;j<180;j++)
//	             {
//	               TX_Mode();								// 发射数据
//	               while (!(TX_DS|MAX_RT));				// 等待发射结束
//	               sta = 0;
//	             }
//	             RX_Mode();
//	            }
//			 } 
//		} 

          

				if(RX_DR)								// 数据已收到
				{
					sta=0;
					/***********组帧上传给串口************/
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
             				