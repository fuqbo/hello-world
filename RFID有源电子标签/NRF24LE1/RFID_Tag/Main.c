/*********************************************************************

功能:
	标签TAG运行程序

说明:
    1.标签主动发送自身信息（ID编号 + 射频发送信号强度）
	2.标签有两种状态可切换:   
	                       状态1：常规主动发送（休眠-接收-发送）
					       状态2：被动接收（休眠-接收）

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
#define	ID_LENGTH	       9        //前8个字节为序列号，第9个字节固定为0XA5表示已写入ID

#define eddr            0xFA00	  //设定射频功率值所在NVM存储器地址

#define SEND_CYCLE      2  //发送次数


#define INTERRUPT_RFIRQ	9
#define INTERRUPT_TICK 	13  

#define TX_ADR_WIDTH    8   			    // RF收发地址共8 bytes 
#define TX_PLOAD_WIDTH  32  					// 数据包长度为32 bytes
#define RX_PLOAD_WIDTH  32  					// 数据包长度为9 bytes

uint8_t const TX_ADDRESS[TX_ADR_WIDTH]  = {0xD2,0xF8,0xD0,0xD0,0xBD,0xA3,0xBF,0xCD}; //定义RF发送地址
uint8_t const RX_ADDRESS[TX_ADR_WIDTH]  = {0xD0,0xEC,0xC8,0xB5,0xC7,0xBF,0x51,0x51};			 //定义RF接收地址

uint8_t	data uid=0x03;      //本标签的ID号(自己烧录)
uint8_t data txpower=0x0f;		//标签射频发射功率
uint8_t data rx_buf[RX_PLOAD_WIDTH];
uint8_t data tx_buf[TX_PLOAD_WIDTH]={0x00, 0x00, 0x11, 0x22, 0x33, 0x66, 0x03, 0x00, 0x00, 0x00, 
                                     0x23, 0x59, 0x13, 0x14, 0x15, 0x16, 0xFF, 0x01, 0x01, 0x01, 
                                     0x00, 0x81, 0x85, 0x08, 0x0F, 0x20, 0x22, 0x00, 0x00, 0x00,
                                     0x00, 0x00};

/*

信号强度   1字节
电量       1字节
卡号       4字节																		 
时间段模式 1字节
前后门     1字节
每日时段   4字节
结束日期   4字节
星期       1字节
楼层权限   12字节
保留       3字节
*/																		 
																		 
																		 
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
功能：设置为掉电模式
**************************************************/
void PD_Mode(void)
{
  	RFCE=0;
  	SPI_RW_Reg(WRITE_REG + CONFIG, 0x0c);   	// PWR_UP=0
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
	SPI_RW_Reg(FLUSH_TX,0);								// 清除TX的FIFO
  	SPI_RW_Reg(WRITE_REG + CONFIG, 0x0e);     			// 上电, CRC为2 bytes,接收模式,允许RX_DR产生中断  	
	SPI_Write_Buf(WR_TX_PLOAD, tx_buf, TX_PLOAD_WIDTH); // 写数据到FIFO
	RFCE=1;	
}
/**************************************************
功能：RF初始化
**************************************************/
void rf_init(void)
{
  	RFCE = 0;                                   		// RF关闭
  	RFCKEN = 1;                                 		// 启动RF时钟
  	RF = 1;                                     		// 允许RF中断
	delay(1000);

  	SPI_Write_Buf(WRITE_REG + TX_ADDR, TX_ADDRESS, TX_ADR_WIDTH);    	// 设置发射地址长度	
	SPI_Write_Buf(WRITE_REG + RX_ADDR_P0, RX_ADDRESS, TX_ADR_WIDTH); 	// 设置接收地址长度 

  	SPI_RW_Reg(WRITE_REG + EN_AA, 0x01);      			// 启动自动应答功能
  	SPI_RW_Reg(WRITE_REG + EN_RXADDR, 0x01);  			// PIPE0接收数据
  	SPI_RW_Reg(WRITE_REG + SETUP_RETR, 0x1a); 			// 自动重传10次
  	SPI_RW_Reg(WRITE_REG + RF_CH, 40);        			// RF频率2440MHz
  	SPI_RW_Reg(WRITE_REG + RF_SETUP, txpower);   	    // 配置发射功率txpower, 传输速率2Mbps,
  	SPI_RW_Reg(WRITE_REG + RX_PW_P0, TX_PLOAD_WIDTH); 	// PIPE0 接收数据包长度		*/	
} 

/**************************************************
功能：RF中断服务程序
**************************************************/
void RF_IRQ(void) interrupt INTERRUPT_RFIRQ
{
	sta=SPI_Read(STATUS);								// 读出状态值
	
	/***************************标签状态切换响应而添加***********************/
	if(RX_DR)									
	{
		SPI_Read_Buf(RD_RX_PLOAD,rx_buf,TX_PLOAD_WIDTH);// 读出FIFO的数据
		SPI_RW_Reg(FLUSH_RX,0);							// 清除RX的FIFO
	}
	/*************************************************************************/

	SPI_RW_Reg(WRITE_REG+STATUS,0x70);					// 清除所有中断标志 
}

/**************************************************
功能：随机数发生器初始化子程序
**************************************************/
//void rng_init(void)
//{
//   RNGCTL=0x80;               //启动随机数发生器
//}

/*配置电压报警寄存器*/
void PWR_CONFIG()
{
POFCON|=0X80;//使能供电失败报警，配置比较电压为2.1v
}

/*读取电源失败的原因*/
unsigned char RD_POF()
{
return (POFCON&=0X10) ;	//如果是0就是高，如是去1就是电压低于比较值
}

//读取写进去的ID
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
		//芯片没写入序列号，使用默认序列号或报错
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
功能：RTC2初始化
**************************************************/
void rtc2_init(void)
{
	CLKLFCTRL=0x01;									   	// 使用RC 32KHz时钟
	
	RTC2CMP0=0xFF;										// 定时2秒
	if(SEND_CYCLE <=5)
	{
		RTC2CMP1=0x20;        //0X80--1S
	
	}
	else
	{
		RTC2CMP1=0xFF;        //0X80--1S
	}

	
	RTC2CON=0x06;										// 比较模式
	WUIRQ=1;											// 允许TICK中断
}
/**************************************************
功能：启动RTC2
**************************************************/
void rtc2_on(void)
{
    //RTC2定时间隔配置
	RTC2CON |=0x01;									
}
/**************************************************
功能：关闭RTC2
**************************************************/
void rtc2_off(void)
{													  	
	RTC2CON &=~0x01;							   		
}
/**************************************************
功能：RTC2中断服务程序
**************************************************/
void RTC2_IRQ(void) interrupt INTERRUPT_TICK 
{
	//LED2=!LED2;										   	
}              				
/**************************************************
功能:I/O口初始化
**************************************************/
void io_init(void)
{
  	P0DIR = p0dir;							   	// 设定I/O口输入输出
  	P1DIR = p1dir;					                
}  

/**************************************************
功能：串口初始化子程序	 *****调试用
说明：波特率9600，使用内部波特率发生器
**************************************************/
//void uart_init(void)
//{
//    ES0 = 0;                      				// 关UART0中断
//    REN0 = 1;                     				// 允许接收
//    SM0 = 0;                      				// 串口模式1，8bit可变波特率
//    SM1 = 1;                   
//    PCON |= 0x80;                 				// SMOD = 1
//	
//    ADCON |= 0x80;                				// 选择内部波特率发生器
//    S0RELL = 0xcc;                				// 波特率9600(十进制972=十六进制0x03cc
//    S0RELH = 0x03;							
//    TI0 = 0;					  				// 清发送完成标志
//	RI0=0;					  				   // 清接收完成标志
//	S0BUF=0x99;					  				// 送初值
//}
/**************************************************
功能：向串口发送1 byte数据	*****调试用
**************************************************/
//void uart_putchar(uint8_t x)
//{
//	while (!TI0);								// 等待发送完成
//	TI0=0;										// 清发送完成标志
//	S0BUF=x;									// 发送数据
//}

/**************************************************
功能：NVM存储器多字节擦除/写入子程序
说明：先擦除所在页，然后写入数据
**************************************************/
void hal_flash_bytes_write(uint16_t a,uint8_t *p,uint16_t n)
{
   uint8_t xdata * data pb;
   if((a>=0xfa00)&&(a<=0xffff))	   //NVM地址区
   {
     F0=EA;
	 EA=0; 
	 WEN=1;             //允许写入
	 if((a>=0xfa00)&&(a<0xfc00))   //地址在NVM加长寿命区
	 {
	    FCR=32;	               //必须擦出整数页（512字节）
		while(RDYN==1);
		FCR=33;
		while(RDYN==1);
	 }
	 if((a>=0xfc00)&&(a<=0xffff))      //地址在NVM普通寿命区
	 {
	   FCR=34;                   //擦除整页  
	   while(RDYN==1);
	 }
	 if((a>=0xfe00)&&(a<=0xffff))      //地址在NVM普通寿命区
	 {
	   FCR=35;                   //擦除整页  
	   while(RDYN==1);
	 }
	 delay(1000);
	
	 pb=(uint8_t xdata *)a;
	 while(n--)				  //连续写入数据
	 {
	   *pb++=*p++;
	   while(RDYN==1);
	 }
	 WEN=0;					  //写完，禁止写
	 EA=F0;				      //恢复中断
   }
}



//设置发射功率+唯一标志  1字节+3字节
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
/nRF24LE1的低功耗模式设置
/
****************/
void  Low_Power_IO_Set(void)
{

	P0CON = 0x70;  //P0.0设置为数字输入缓冲关模式
	P0CON = 0x71;  //P0.1设置为数字输入缓冲关模式
	P0CON = 0x72;  //P0.2设置为数字输入缓冲关模式
	P0CON = 0x73;  //P0.3设置为数字输入缓冲关模式
	P0CON = 0x74;  //P0.4设置为数字输入缓冲关模式
	P0CON = 0x75;  //P0.5设置为数字输入缓冲关模式
	P0CON = 0x76;  //P0.6设置为数字输入缓冲关模式
	P0CON = 0x77;  //P0.7设置为数字输入缓冲关模式
	P0DIR = 0xFF;  //P0.0~P0.7方向设置为输入
	
	
	P1CON = 0x70;  //P1.0设置为数字输入缓冲关模式
	P1CON = 0x71;  //P1.1设置为数字输入缓冲关模式
	P1CON = 0x72;  //P1.2设置为数字输入缓冲关模式
	P1CON = 0x73;  //P1.3设置为数字输入缓冲关模式
	P1CON = 0x74;  //P1.4设置为数字输入缓冲关模式
	P1CON = 0x75;  //P1.5设置为数字输入缓冲关模式
	P1CON = 0x76;  //P1.6设置为数字输入缓冲关模式

	P1DIR = 0x7F;  //P1.0~P1.6方向设置为输入

}
	

/**************************************************
功能：主程序
**************************************************/
void main(void)
{	
	
	uint16_t  cycle = 0 ,jj = 0;
	
  uint8_t   i = 0, j=0 , k=0 ,kk = 0;

	io_init();									// I/O口初始化

	LED0=1;            //点灯
	
//	P1DIR = 0XFF;		   //配置成输入
	
  PWR_CONFIG();  	/*配置电压报警寄存器*/
	
  read_id_from_nvdata();  	//读取写进去的ID
	
	//配置初始发射功率
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
	
	tx_buf[0] =  txpower ;     //信号强度
	
	rf_init();									// RF初始化                            
  EA=1;                       // 允许中断	 
	rtc2_init();                // RTC2初始化
			
	if(RD_POF())					 //检测反馈当前的供电的电压情况  
		{	
		tx_buf[1]  = 0x00 ;     //低压
		}
  else
	 {
		tx_buf[1]  = 0x80 ;
	 }
	 
	 RX_Mode(); 	  	//进入接收模式
	 
	while(1)
	{

			rtc2_off();								// 关RTC2	
		
		if(jj >1500)
		{
					
			
			
			
		/**********************状态1：常规主动发送（休眠-接收-发送）************************/
 
		   sta=0;	
			 delay(10);
		 
			TX_Mode();								// 发射本TAG有源ID数据				
		     while (!(TX_DS|MAX_RT));				// 等待发射结束			
			sta = 0; 				
			
			PD_Mode();								// 关RF
							
			if(!kk)	{	  kk ++;	}		
				
			tx_buf[1]  |= kk ;     //	
								
			if(cycle>=SEND_CYCLE)    //2s/次 150 5分钟    
				{ 
				
					Low_Power_IO_Set();
					
				  OPMCON |= 0x02;          //I/O状态锁存
					
          PWRDWN = 0x01;           //进入深度睡眠模式，唤醒会导致复位
				}	
  
       else
			 {			
            cycle ++ ;
				 
			 		  rtc2_on();								// 开RTC2			 
				 				 
				 	  Low_Power_IO_Set();
				 
				 		PWRDWN = 0x04;							// 进入低功耗模式3.2uA，寄存器维持，clklf来自RCOSC32K，等待RTC2唤醒				 
			 }  
		}
		else
		{
			
			jj ++;
			delay(1);
		
			if(RX_DR)								// 数据已收到
				{					
					sta = 0;
					
				if(((rx_buf[0]==tx_buf[2])&&(rx_buf[1]==tx_buf[3])&&(rx_buf[2]==tx_buf[4])&&(rx_buf[3]==tx_buf[5]))||((rx_buf[0]==0xFF)&&(rx_buf[1]==0xFF)&&(rx_buf[2]==0xFF)&&(rx_buf[3]==0xFF)))				
		      {	
											
						switch (rx_buf[4]) //处理命令
							{
								
										case 0xAA: set_rf_txpower();                     break;     //设置发射功率
		//								case 0x02: GetVersion();                      break;     //读取版本信息
		//								case 0x03: SetMachno();                       break;     //设置机号
		//								case 0x04: GetMachno();                       break;     //读取机号		
		//								case 0x05: SetTime();                         break;     //设置时间
								
										default:  break;
							}				
					}	
					
				}	
		
		}
	
		  /*************************** 状态2：被动接收（休眠-接收）******************************/
//		  if(rx_buf[0]==0x22)								// 数据已收到
//		  {	
//			sta=0;
//			delay(15);
//			PD_Mode();								// 关RF
//		    rtc2_on();								// 开RTC2
//		    PWRDWN = 0x04;							// 进入低功耗模式，等待RTC2唤醒
//		    RX_Mode(); 
//			memrx=0x22;	
//				
//				
//				
//		  }

		  /*****************接收射频功率配置指令,配置标签发射功率,并写入NVM存储区****************/
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


