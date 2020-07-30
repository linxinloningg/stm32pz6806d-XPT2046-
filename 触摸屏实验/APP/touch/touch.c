#include "touch.h"
#include "SysTick.h"
#include "tftlcd.h"
#include "spi.h"
#include "usart.h"

#define TOUCH_AdjDelay500ms() delay_ms(500)

TouchTypeDef TouchData;         //���������洢��ȡ��������
static PosTypeDef TouchAdj;     //����һ��������������У������
         

void TOUCH_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

    /* SPI��IO�ں�SPI�����ʱ�� */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);

    /* TOUCH-CS��IO������ */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(GPIOD, &GPIO_InitStructure);

    /* TOUCH-PEN��IO������ */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;

    GPIO_Init(GPIOD, &GPIO_InitStructure);

    SPI1_Init();
	if(TouchAdj.posState != TOUCH_ADJ_OK)
    {
        TOUCH_Adjust(); //У��   
    }
}



uint16_t TOUCH_ReadData(uint8_t cmd)
{
    uint8_t i, j;
    uint16_t readValue[TOUCH_READ_TIMES], value;
    uint32_t totalValue;
	
	/* SPI���ٶȲ��˹��� */
    SPI1_SetSpeed(SPI_BaudRatePrescaler_16);
	
    /* ��ȡTOUCH_READ_TIMES�δ���ֵ */
    for(i=0; i<TOUCH_READ_TIMES; i++)
    {   /* ��Ƭѡ */
        TCS=0;
        /* �ڲ��ģʽ�£�XPT2046ת����Ҫ24��ʱ�ӣ�8��ʱ���������֮��1��ʱ��ȥ�� */
        /* æ�źţ��������12λת�������ʣ��3��ʱ���Ǻ���λ */    
        SPI1_ReadWriteByte(cmd); // �������ѡ��X�����Y�� 
        
        /* ��ȡ���� */
        readValue[i] = SPI1_ReadWriteByte(0xFF);
        readValue[i] <<= 8;
        readValue[i] |= SPI1_ReadWriteByte(0xFF);
        
        /* �����ݴ�����ȡ����ADֵ��ֻ��12λ�������λ���� */
        readValue[i] >>= 3;
        
        TCS=1;
    }

    /* �˲����� */
    /* ���ȴӴ�С���� */
    for(i=0; i<(TOUCH_READ_TIMES - 1); i++)
    {
        for(j=i+1; j<TOUCH_READ_TIMES; j++)
        {
            /* ����ֵ�Ӵ�С�������� */
            if(readValue[i] < readValue[j])
            {
                value = readValue[i];
				readValue[i] = readValue[j];
				readValue[j] = value;
            }   
        }       
    }
   
    /* ȥ�����ֵ��ȥ����Сֵ����ƽ��ֵ */
    j = TOUCH_READ_TIMES - 1;
    totalValue = 0;
    for(i=1; i<j; i++)     //��y��ȫ��ֵ
    {
        totalValue += readValue[i];
    }
    value = totalValue / (TOUCH_READ_TIMES - 2);
      
    return value;
}

uint8_t TOUCH_ReadXY(uint16_t *xValue, uint16_t *yValue)
{   
    uint16_t xValue1, yValue1, xValue2, yValue2;

    xValue1 = TOUCH_ReadData(TOUCH_X_CMD);
    yValue1 = TOUCH_ReadData(TOUCH_Y_CMD);
    xValue2 = TOUCH_ReadData(TOUCH_X_CMD);
    yValue2 = TOUCH_ReadData(TOUCH_Y_CMD);
    
    /* �鿴������֮���ֻ����ֵ��� */
    if(xValue1 > xValue2)
    {
        *xValue = xValue1 - xValue2;
    }
    else
    {
        *xValue = xValue2 - xValue1;
    }

    if(yValue1 > yValue2)
    {
        *yValue = yValue1 - yValue2;
    }
    else
    {
        *yValue = yValue2 - yValue1;
    }
	
    /* �жϲ�����ֵ�Ƿ��ڿɿط�Χ�� */
	if((*xValue > TOUCH_MAX+0) || (*yValue > TOUCH_MAX+0))  
	{
		return 0xFF;
	}

    /* ��ƽ��ֵ */
    *xValue = (xValue1 + xValue2) / 2;
    *yValue = (yValue1 + yValue2) / 2;

    /* �жϵõ���ֵ���Ƿ���ȡֵ��Χ֮�� */
    if((*xValue > TOUCH_X_MAX+0) || (*xValue < TOUCH_X_MIN) 
       || (*yValue > TOUCH_Y_MAX+0) || (*yValue < TOUCH_Y_MIN))
    {                   
        return 0xFF;
    }
 
    return 0; 
}
void TOUCH_Adjust(void)
{
    float xFactor, yFactor;
    /* ����������� */
    xFactor = (float)LCD_ADJ_X / (TOUCH_X_MAX - TOUCH_X_MIN);
    yFactor = (float)LCD_ADJ_Y / (TOUCH_Y_MAX - TOUCH_Y_MIN);  
    
    /* ���ƫ���� */
    TouchAdj.xOffset = (int16_t)LCD_ADJX_MAX - ((float)TOUCH_X_MAX * xFactor);
    TouchAdj.yOffset = (int16_t)LCD_ADJY_MAX - ((float)TOUCH_Y_MAX * yFactor);

    /* �����������������ݴ���Ȼ�󱣴� */
    TouchAdj.xFactor = xFactor ;
    TouchAdj.yFactor = yFactor ;
    
    TouchAdj.posState = TOUCH_ADJ_OK;           
}


uint8_t TOUCH_Scan(void)
{
    
    //if(PEN == 0)   //�鿴�Ƿ��д���
    {
        if(TOUCH_ReadXY(&TouchData.x, &TouchData.y)) //û�д���
        {
            return 0xFF;    
        }
        /* ������������ֵ���������������ֵ */
        TouchData.lcdx = TouchData.x * TouchAdj.xFactor + TouchAdj.xOffset;
        TouchData.lcdy = TouchData.y * TouchAdj.yFactor + TouchAdj.yOffset;
        
        /* �鿴��������ֵ�Ƿ񳬹�������С */
        if(TouchData.lcdx > tftlcd_data.width)
        {
            TouchData.lcdx = tftlcd_data.width;
        }
        if(TouchData.lcdy > tftlcd_data.height)
        {
            TouchData.lcdy = tftlcd_data.height;
        }
        return 0; 
    }
 //   return 0xFF;       
}







