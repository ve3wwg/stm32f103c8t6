/* monitor.c : Monitor Program/Routine
 * Warren W. Gay VE3WWG
 * Sun May 21 18:45:28 2017
 */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/f1/bkp.h>
#include <libopencm3/stm32/f1/nvic.h>
#include <libopencm3/stm32/can.h>

#include <FreeRTOS.h>
#include <mcuio.h>
#include <miniprintf.h>
#include <monitor.h>

enum Format {
        Binary=0,
        Decimal,
        Hex,
};

struct bdesc {
        unsigned        sbit : 5;
        unsigned        bits : 6;
        enum Format     format : 3;
        int             width : 6;
        const char      *desc;
};

static void
putbin(char *buf,uint32_t v,unsigned bits) {
        char temp[34], *p;

        p = temp + sizeof temp;
        for ( *--p = 0; bits > 0; --bits ) {
                *--p = (v & 1) ? '1' : '0';
                v >>= 1;
        }
        strcpy(buf,p);
}

static int
dump_hdrs(int x,const struct bdesc *desc,int n) {
        int w, tw, b, first=x;

        std_printf("  ");
        for ( ; x<n; ++x ) {
                w = desc[x].width;
                tw = strlen(desc[x].desc);

                if ( x > first && w < 0 ) {
                        std_putc('\n');
                        return x;
                } else if ( w < 0 )
                        w = -w;

                if ( tw > w )
                        b = tw - w;
                else    b = w - tw;

                while ( b-- > 0 )
                        std_putc(' ');

                std_printf("%s%s",
                        desc[x].desc,
                        x+1 < n ? "|" : "\n");
        }
        return 0;
}

static void
dump_vals(int x,uint32_t reg,const struct bdesc *desc,int n) {
        char buf[40];
        uint32_t v, mask, shiftr;
        int w, tw, b, first=x;

        std_printf("  ");
        for ( ; x<n; ++x ) {
                w = desc[x].width;
                if ( x>first && w < 0 ) {
                        std_putc('\n');
                        break;
                } else if ( w < 0 )
                        w = -w;

                mask = (1 << desc[x].bits) - 1;
                shiftr = desc[x].sbit - desc[x].bits + 1;
                v = (reg >> shiftr) & mask;
                switch ( desc[x].format ) {
                case Binary:
                        putbin(buf,v,desc[x].bits);
                        break;
                case Decimal:
                        mini_snprintf(buf,sizeof buf,"%d",(int)v);
                        break;
                case Hex:
                        mini_snprintf(buf,sizeof buf,"$%X",(unsigned)v);
                        break;
                default:
                        buf[0] = '?';
                        buf[1] = 0;
                }

                tw = strlen(buf);

                if ( tw > w )
                        b = tw - w;
                else    b = w - tw;

                while ( b-- > 0 )
                        std_putc(' ');

                std_printf("%s%c",buf,x+1 < n ? '|' : '\n');
        }
}

static void
dump_reg_info(volatile uint32_t *raddr,const char *dev,int no,const char *descrip) {
        uint32_t reg = *raddr;
        char name[32];

        if ( no > 0 )
                mini_snprintf(name,sizeof name,"%s%d_%s",dev,no,descrip);
        else    mini_snprintf(name,sizeof name,"%s%s%s",
                        dev,
                        descrip ? "_" : "",
                        descrip ? descrip : "");
        std_printf("\n%-12s: $%08X @ $%08X\n",name,(unsigned)reg,(unsigned)raddr);
}

static void
dump_reg_simple16(volatile uint32_t *raddr,const char *dev,int no,const char *descrip,uint16_t value) {
        uint32_t reg = *raddr;
        char name[32];

        if ( no > 0 )
                mini_snprintf(name,sizeof name,"%s%d_%s",dev,no,descrip);
        else    mini_snprintf(name,sizeof name,"%s%s%s",
                        dev,
                        descrip ? "_" : "",
                        descrip ? descrip : "");
        std_printf("%-12s: $%08X @ $%08X, VALUE: $%08X  %5u",name,(unsigned)reg,(unsigned)raddr,(unsigned)value,(unsigned)value);
        if ( value & 0x8000 )
                std_printf("  (%d)",(int)value);
        std_printf("\n");
}

static void
dump_reg(volatile uint32_t *raddr,const char *dev,int no,const char *descrip,const struct bdesc *desc,int n) {
        uint32_t reg = *raddr;
        int x, x2;

        dump_reg_info(raddr,dev,no,descrip);
        x = dump_hdrs(0,desc,n);
        dump_vals(0,reg,desc,n);

        for ( ; x > 0; x = x2 ) {
                x2 = dump_hdrs(x,desc,n);
                dump_vals(x,reg,desc,n);
        }
}

static void
dump_rcc(void) {
        static const struct bdesc rcc_cr[] = {
                { 31, 6, Binary, 6, "res" },
                { 25, 1, Binary, 6, "PLLRDY" },
                { 24, 1, Binary, 5, "PLLON" },
                { 23, 4, Binary, 4, "res" },
                { 19, 1, Binary, 5, "CSSON" },
                { 18, 1, Binary, 6, "HSEBYP" },
                { 17, 1, Binary, 6, "HSERDY" },
                { 16, 1, Binary, 5, "HSEON" },
                { 15, 8, Hex, 6, "HSICAL" },
                { 7, 5, Hex, 7, "HSITRIM" },
                { 2, 1, Binary, 3, "res" },
                { 1, 1, Binary, 6, "HSIRDY" },
                { 0, 1, Binary, 5, "HSION" }
        };
        static const struct bdesc rcc_cfgr[] = {
                { 31, 5, Binary, 5, "res" },
                { 26, 3, Binary, 3, "MCO" },
                { 23, 1, Binary, 3, "res" },
                { 22, 1, Binary, 6, "USBPRE" },
                { 21, 4, Binary, 6, "PLLMUL" },
                { 17, 1, Binary, 8, "PLLXTPRE" },
                { 16, 1, Binary, 6, "PLLSRC" },
                { 15, 2, Binary, 6, "ADCPRE" },
                { 13, 3, Binary, 5, "PPRE2" },
                { 10, 3, Binary, 5, "PPRE1" },
                { 7, 4, Binary, 4, "HPRE" },
                { 3, 2, Binary, 3, "SWS" },
                { 1, 2, Binary, 2, "SW" }
        };
        static const struct bdesc rcc_cir[] = {
                { 31, 8, Binary, 8, "res" },
                { 23, 1, Binary, 4, "CSSC" },
                { 22, 2, Binary, 3, "res" },
                { 20, 1, Binary, 7, "PLLRDYC" },
                { 19, 1, Binary, 7, "HSERDYC" },
                { 18, 1, Binary, 7, "HSIRDYC" },
                { 17, 1, Binary, 7, "LSERDYC" },
                { 16, 1, Binary, 7, "LSIRDYC" },
                { 15, 3, Binary, -3, "res" },
                { 12, 1, Binary, 8, "PLLRDYIE" },
                { 11, 1, Binary, 8, "HSERDYIE" },
                { 10, 1, Binary, 8, "HSIRDYIE" },
                { 9, 1, Binary, 8, "LSERDYIE" },
                { 8, 1, Binary, 8, "LSIRDYIE" },
                { 7, 1, Binary, 4, "CSSF" },
                { 6, 2, Binary, 3, "res" },
                { 4, 1, Binary, 7, "PLLRDYF" },
                { 3, 1, Binary, 7, "HSERDYF" },
                { 2, 1, Binary, 7, "HSIRDYF" },
                { 1, 1, Binary, 7, "LSERDYF" },
                { 0, 1, Binary, 7, "LSIRDYF" },
        };
        static const struct bdesc rcc_apb2rstr[] = {
                { 31, 10, Binary, 10, "res" },
                { 21, 1, Binary, 8, "TIM11RST" },
                { 20, 1, Binary, 8, "TIM10RST" },
                { 19, 1, Binary, 7, "TIM9RST" },
                { 18, 3, Binary, 3, "res" },
                { 15, 1, Binary, -7, "ADC3RST" },
                { 14, 1, Binary, 9, "USART1RST" },
                { 13, 1, Binary, 7, "TIM8RST" },
                { 12, 1, Binary, 7, "SPI1RST" },
                { 11, 1, Binary, 7, "TIM1RST" },
                { 10, 1, Binary, 7, "ADC2RST" },
                { 9, 1, Binary, 7, "ADC1RST" },
                { 8, 1, Binary, 7, "IOPGRST" },
                { 7, 1, Binary, 7, "IOPFRST" },
                { 6, 1, Binary, 7, "IOPERST" },
                { 5, 1, Binary, 7, "IOPDRST" },
                { 4, 1, Binary, 7, "IOPCRST" },
                { 3, 1, Binary, 7, "IOPBRST" },
                { 2, 1, Binary, 7, "IOPARST" },
                { 1, 1, Binary, 3, "res" },
                { 0, 1, Binary, 7, "AFIORST" }
        };
        static const struct bdesc rcc_ahbenr[] = {
                { 31, 21, Binary, 21, "res" },
                { 10, 1, Binary, 6, "SDIOEN" },
                { 9, 1, Binary, 3, "res" },
                { 8, 1, Binary, 6, "FSMCEN" },
                { 7, 1, Binary, 3, "res" },
                { 6, 1, Binary, 5, "CRCEN" },
                { 5, 1, Binary, 3, "res" },
                { 4, 1, Binary, 7, "FLITFEN" },
                { 3, 1, Binary, 3, "res" },
                { 2, 1, Binary, 6, "SRAMEN" },
                { 1, 1, Binary, 6, "DMA2EN" },
                { 0, 1, Binary, 6, "DMA1EN" }
        };
        static const struct bdesc rcc_apb2enr[] = {
                { 31, 10, Binary, 10, "res" },
                { 21, 1, Binary,  7, "TIM11EN" },
                { 20, 1, Binary,  7, "TIM10EN" },
                { 19, 1, Binary,  6, "TIM9EN" },
                { 18, 3, Binary,  3, "res" },
                { 15, 1, Binary,  -6, "ADC3EN" },
                { 14, 1, Binary,  8, "USART1EN" },
                { 13, 1, Binary,  6, "TIM8EN" },
                { 12, 1, Binary,  6, "SPI1EN" },
                { 11, 1, Binary,  6, "TIM1EN" },
                { 10, 1, Binary,  6, "ADC2EN" },
                {  9, 1, Binary,  6, "ADC1EN" },
                {  8, 1, Binary,  6, "IOPGEN" },
                {  7, 1, Binary,  6, "IOPFEN" },
                {  6, 1, Binary,  6, "IOPEEN" },
                {  5, 1, Binary,  6, "IOPDEN" },
                {  4, 1, Binary,  6, "IOPCEN" },
                {  3, 1, Binary,  6, "IOPBEN" },
                {  2, 1, Binary,  6, "IOPAEN" },
                {  1, 1, Binary,  3, "res" },
                {  0, 1, Binary,  6, "AFIOEN" }
        };
        static const struct bdesc rcc_apb1enr[] = {
                { 31, 2, Binary, 3, "res" },
                { 29, 1, Binary,  5, "DACEN" },
                { 28, 1, Binary,  5, "PWREN" },
                { 27, 1, Binary,  5, "BCKEN" },
                { 26, 1, Binary,  3, "res" },
                { 25, 1, Binary,  5, "CANEN" },
                { 24, 1, Binary,  3, "res" },
                { 23, 1, Binary,  5, "USBEN" },
                { 22, 1, Binary,  6, "I2C2EN" },
                { 21, 1, Binary,  6, "I2C1EN" },
                { 20, 1, Binary,  7, "UART5EN" },
                { 19, 1, Binary,  7, "UART4EN" },
                { 18, 1, Binary,  8, "USART3EN" },
                { 17, 1, Binary,  8, "USART2EN" },
                { 16, 1, Binary,  -3, "res" },
                { 15, 1, Binary,  6, "SPI3EN" },
                { 14, 1, Binary,  6, "SPI2EN" },
                { 13, 2, Binary,  3, "res" },
                { 11, 1, Binary,  6, "WWDGEN" },
                { 10, 2, Binary,  3, "res" },
                {  8, 1, Binary,  7, "TIM14EN" },
                {  7, 1, Binary,  7, "TIM13EN" },
                {  6, 1, Binary,  7, "TIM12EN" },
                {  5, 1, Binary,  6, "TIM7EN" },
                {  4, 1, Binary,  6, "TIM6EN" },
                {  3, 1, Binary,  6, "TIM5EN" },
                {  2, 1, Binary,  6, "TIM4EN" },
                {  1, 1, Binary,  6, "TIM3EN" },
                {  0, 1, Binary,  6, "TIM2EN" },
        };
        static const struct bdesc rcc_bdcr[] = {
                { 31, 15, Binary, 15, "res" },
                { 16,  1, Binary,  5, "BD$ST" },
                { 15,  1, Binary,  5, "res" },
                {  9,  2, Binary,  6, "RTCSEL" },
                {  7,  5, Binary,  5, "res" },
                {  2,  1, Binary,  6, "LSEBYP" },
                {  1,  1, Binary,  6, "LSERDY" },
                {  0,  1, Binary,  5, "LSEON" },
        };
        static const struct bdesc rcc_csr[] = {
                { 31,  1, Binary,  8, "LPWRRSTF" },
                { 30,  1, Binary,  8, "WWDGRSTF" },
                { 29,  1, Binary,  8, "IWDGRSTF" },
                { 28,  1, Binary,  7, "SFTRSTF" },
                { 27,  1, Binary,  7, "PORRSTF" },
                { 26,  1, Binary,  7, "PINRSTF" },
                { 25,  1, Binary,  3, "res" },
                { 24,  1, Binary,  4, "RMVF" },
                { 23,  8, Binary,  8, "res" },
                { 15, 14, Binary, 14, "res" },
                {  1,  1, Binary,  6, "LSIRDY" },
                {  0,  1, Binary,  5, "LSION" },
        };

        dump_reg(&RCC_CR,"RCC",0,"CR",rcc_cr,13);
        dump_reg(&RCC_CFGR,"RCC",0,"CFGR",rcc_cfgr,13);
        dump_reg(&RCC_CIR,"RCC",0,"CIR",rcc_cir,21);
        dump_reg(&RCC_APB2RSTR,"RCC",0,"APB2RSTR",rcc_apb2rstr,21);
        dump_reg(&RCC_AHBENR,"RCC",0,"AHBENR",rcc_ahbenr,12);
        dump_reg(&RCC_APB2ENR,"RCC",0,"APB2ENR",rcc_apb2enr,21);
        dump_reg(&RCC_APB1ENR,"RCC",0,"APB1ENR",rcc_apb1enr,29);
        dump_reg(&RCC_BDCR,"RCC",0,"BDCR",rcc_bdcr,8);
        dump_reg(&RCC_CSR,"RCC",0,"CSR",rcc_csr,12);
}

static void
dump_gpio(void) {
        static const struct bdesc gpiox_crlh[] = {
                { 31, 2, Binary, 4, "CNF7" },
                { 29, 2, Binary, 5, "MODE7" },
                { 27, 2, Binary, 4, "CNF6" },
                { 25, 2, Binary, 5, "MODE6" },
                { 23, 2, Binary, 4, "CNF5" },
                { 21, 2, Binary, 5, "MODE5" },
                { 19, 2, Binary, 4, "CNF4" },
                { 17, 2, Binary, 5, "MODE4" },
                { 15, 2, Binary, 4, "CNF3" },
                { 13, 2, Binary, 5, "MODE3" },
                { 11, 2, Binary, 4, "CNF2" },
                {  9, 2, Binary, 5, "MODE2" },
                {  7, 2, Binary, 4, "CNF1" },
                {  5, 2, Binary, 5, "MODE1" },
                {  3, 2, Binary, 4, "CNF0" },
                {  1, 2, Binary, 5, "MODE0" },
        };

        dump_reg(&GPIOA_CRL,"GPIOA",0,"CRL",gpiox_crlh,16);
        dump_reg(&GPIOB_CRL,"GPIOB",0,"CRL",gpiox_crlh,16);
        dump_reg(&GPIOC_CRL,"GPIOC",0,"CRL",gpiox_crlh,16);
        dump_reg(&GPIOD_CRL,"GPIOD",0,"CRL",gpiox_crlh,16);

        dump_reg(&GPIOA_CRH,"GPIOA",0,"CRH",gpiox_crlh,16);
        dump_reg(&GPIOB_CRH,"GPIOB",0,"CRH",gpiox_crlh,16);
        dump_reg(&GPIOC_CRH,"GPIOC",0,"CRH",gpiox_crlh,16);
        dump_reg(&GPIOD_CRH,"GPIOD",0,"CRH",gpiox_crlh,16);

        std_printf("\n  CNFx In 00=Analog,    01=Floating input, 10=Input Pull-up/down, 11= Reserved\n");
        std_printf(  "      Out 00=Push/Pull, 01=Open-Drain,     10=AF Push/Pull,       11=AF Open Drain\n");
        std_printf(  "  MODEy:  00=Input,     01=Output 10 MHz,  10=Output 2 MHz,       11=Output 50 MHz\n");
}

static void
dump_gpio_inputs(void) {
        static const struct bdesc gpiox_idr[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  1, Binary,  5, "IDR15" },
                { 14,  1, Binary,  5, "IDR14" },
                { 13,  1, Binary,  5, "IDR13" },
                { 12,  1, Binary,  5, "IDR12" },
                { 11,  1, Binary,  5, "IDR11" },
                { 10,  1, Binary,  5, "IDR10" },
                {  9,  1, Binary,  4, "IDR9" },
                {  8,  1, Binary,  4, "IDR8" },
                {  7,  1, Binary,  4, "IDR7" },
                {  6,  1, Binary,  4, "IDR6" },
                {  5,  1, Binary,  4, "IDR5" },
                {  4,  1, Binary,  4, "IDR4" },
                {  3,  1, Binary,  4, "IDR3" },
                {  2,  1, Binary,  4, "IDR2" },
                {  1,  1, Binary,  4, "IDR1" },
                {  0,  1, Binary,  4, "IDR0" },
        };

        dump_reg(&GPIOA_IDR,"GPIOA",0,"IDR",gpiox_idr,17);
        dump_reg(&GPIOB_IDR,"GPIOB",0,"IDR",gpiox_idr,17);
        dump_reg(&GPIOC_IDR,"GPIOC",0,"IDR",gpiox_idr,17);
        dump_reg(&GPIOD_IDR,"GPIOD",0,"IDR",gpiox_idr,17);
}

static void
dump_gpio_outputs(void) {
        static const struct bdesc gpiox_odr[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  1, Binary,  5, "ODR15" },
                { 14,  1, Binary,  5, "ODR14" },
                { 13,  1, Binary,  5, "ODR13" },
                { 12,  1, Binary,  5, "ODR12" },
                { 11,  1, Binary,  5, "ODR11" },
                { 10,  1, Binary,  5, "ODR10" },
                {  9,  1, Binary,  4, "ODR9" },
                {  8,  1, Binary,  4, "ODR8" },
                {  7,  1, Binary,  4, "ODR7" },
                {  6,  1, Binary,  4, "ODR6" },
                {  5,  1, Binary,  4, "ODR5" },
                {  4,  1, Binary,  4, "ODR4" },
                {  3,  1, Binary,  4, "ODR3" },
                {  2,  1, Binary,  4, "ODR2" },
                {  1,  1, Binary,  4, "ODR1" },
                {  0,  1, Binary,  4, "ODR0" },
        };

        dump_reg(&GPIOA_ODR,"GPIOA",0,"ODR",gpiox_odr,17);
        dump_reg(&GPIOB_ODR,"GPIOB",0,"ODR",gpiox_odr,17);
        dump_reg(&GPIOC_ODR,"GPIOC",0,"ODR",gpiox_odr,17);
        dump_reg(&GPIOD_ODR,"GPIOD",0,"ODR",gpiox_odr,17);
}

static void
dump_gpio_locks(void) {
        static const struct bdesc gpiox_lckr[] = {
                { 31, 15, Binary, 15, "res" },
                { 16,  1, Binary,  4, "LCKK" },
                { 15,  1, Binary,  5, "LCK15" },
                { 14,  1, Binary,  5, "LCK14" },
                { 13,  1, Binary,  5, "LCK13" },
                { 12,  1, Binary,  5, "LCK12" },
                { 11,  1, Binary,  5, "LCK11" },
                { 10,  1, Binary,  5, "LCK10" },
                {  9,  1, Binary,  4, "LCK9" },
                {  8,  1, Binary,  4, "LCK8" },
                {  7,  1, Binary,  4, "LCK7" },
                {  6,  1, Binary,  4, "LCK6" },
                {  5,  1, Binary,  4, "LCK5" },
                {  4,  1, Binary,  4, "LCK4" },
                {  3,  1, Binary,  4, "LCK3" },
                {  2,  1, Binary,  4, "LCK2" },
                {  1,  1, Binary,  4, "LCK1" },
                {  0,  1, Binary,  4, "LCK0" },
        };

        dump_reg(&GPIOA_LCKR,"GPIOA",0,"LCKR",gpiox_lckr,18);
        dump_reg(&GPIOB_LCKR,"GPIOB",0,"LCKR",gpiox_lckr,18);
        dump_reg(&GPIOC_LCKR,"GPIOC",0,"LCKR",gpiox_lckr,18);
        dump_reg(&GPIOD_LCKR,"GPIOD",0,"LCKR",gpiox_lckr,18);
}

static void
dump_afio(void) {
        static const struct bdesc afio_evcr[] = {
                { 31, 15, Binary, 15, "res" },
                { 15,  8, Binary,  8, "res" },
                {  7,  1, Binary,  4, "EVOE" },
                {  6,  3, Binary,  4, "PORT" },
                {  3,  3, Binary,  3, "PIN" }
        };
        static const struct bdesc afio_mapr[] = {
                { 31,  5, Binary,   5, "res" },
                { 26,  3, Binary,   7, "SQW_CFG" },
                { 23,  3, Binary,   3, "res" },
                { 20,  1, Binary,  16, "ADCETRGREG_REMAP" },
                { 19,  1, Binary, -18, "ADCw_ETRGINJ_REMAP" },
                { 18,  1, Binary,  17, "ADC1_TRGREG_REMAP" },
                { 17,  1, Binary,  18,  "ADC1_ETRGINJ_REMAP" },
                { 16,  1, Binary,   3, "res" },
                { 15,  1, Binary, -11, "PD001_REMAP" },
                { 14,  2, Binary,   9, "CAN_REMAP" },
                { 12,  1, Binary,  10, "TIM4_REMAP" },
                { 11,  2, Binary,  10, "TIM3_REMAP" },
                {  9,  2, Binary,  10, "TIM2_REMAP" },
                {  7,  2, Binary,  10, "TIM1_REMAP" },
                {  5,  2, Binary, -12, "USART3_REMAP" },
                {  3,  1, Binary,  12, "USART2_REMAP" },
                {  2,  1, Binary,  12, "USART1_REMAP" },
                {  1,  1, Binary,  10, "I2C1_REMAP" },
                {  0,  1, Binary,  10, "SPI1_REMAP" },
        };
        static const struct bdesc afio_exticr1[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  4, Binary,  5, "EXTI3" },
                { 11,  4, Binary,  5, "EXTI2" },
                {  7,  4, Binary,  5, "EXTI1" },
                {  3,  4, Binary,  5, "EXTI0" },
        };
        static const struct bdesc afio_exticr2[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  4, Binary,  5, "EXTI7" },
                { 11,  4, Binary,  5, "EXTI6" },
                {  7,  4, Binary,  5, "EXTI5" },
                {  3,  4, Binary,  5, "EXTI4" },
        };
        static const struct bdesc afio_exticr3[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  4, Binary,  6, "EXTI11" },
                { 11,  4, Binary,  6, "EXTI10" },
                {  7,  4, Binary,  5, "EXTI9" },
                {  3,  4, Binary,  5, "EXTI8" },
        };
        static const struct bdesc afio_exticr4[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  4, Binary,  6, "EXTI15" },
                { 11,  4, Binary,  6, "EXTI14" },
                {  7,  4, Binary,  6, "EXTI13" },
                {  3,  4, Binary,  6, "EXTI12" },
        };

        dump_reg(&AFIO_EVCR,"AFIO",0,"EVCR",afio_evcr,5);
        std_printf("  PORT: 000=A, 001=B, 010=C, 011=D. 100=E\n");
        dump_reg(&AFIO_MAPR,"AFIO",0,"MAPR",afio_mapr,19);

        dump_reg(&AFIO_EXTICR1,"AFIO",0,"EXTICR1",afio_exticr1,5);
        dump_reg(&AFIO_EXTICR2,"AFIO",0,"EXTICR2",afio_exticr2,5);
        dump_reg(&AFIO_EXTICR3,"AFIO",0,"EXTICR3",afio_exticr3,5);
        dump_reg(&AFIO_EXTICR4,"AFIO",0,"EXTICR4",afio_exticr4,5);
        std_printf("  EXTIx: 0000=A, 0001=B, 0010=C, 0011=D\n");
}

static void
dump_intr(void) {
        static const struct bdesc exti_imr[] = {
                { 31, 12, Binary, 12, "res" },
                { 19,  1, Binary,  3, "res" },
                { 18,  1, Binary,  4, "MR18" },
                { 17,  1, Binary,  4, "MR17" },
                { 16,  1, Binary,  4, "MR16" },
                { 15,  1, Binary,  4, "MR15" },
                { 14,  1, Binary,  4, "MR14" },
                { 13,  1, Binary,  4, "MR13" },
                { 12,  1, Binary,  4, "MR12" },
                { 11,  1, Binary,  4, "MR11" },
                { 10,  1, Binary,  4, "MR10" },
                {  9,  1, Binary,  3, "MR9" },
                {  8,  1, Binary,  3, "MR8" },
                {  7,  1, Binary,  3, "MR7" },
                {  6,  1, Binary,  3, "MR6" },
                {  5,  1, Binary,  3, "MR5" },
                {  4,  1, Binary,  3, "MR4" },
                {  3,  1, Binary,  3, "MR3" },
                {  2,  1, Binary,  3, "MR2" },
                {  1,  1, Binary,  3, "MR1" },
                {  0,  1, Binary,  3, "MR0" },
        };
        static const struct bdesc exti_rtsr[] = {
                { 31, 12, Binary, 12, "res" },
                { 19,  1, Binary,  3, "res" },
                { 18,  1, Binary,  4, "TR18" },
                { 17,  1, Binary,  4, "TR17" },
                { 16,  1, Binary,  4, "TR16" },
                { 15,  1, Binary,  4, "TR15" },
                { 14,  1, Binary,  4, "TR14" },
                { 13,  1, Binary,  4, "TR13" },
                { 12,  1, Binary,  4, "TR12" },
                { 11,  1, Binary,  4, "TR11" },
                { 10,  1, Binary,  4, "TR10" },
                {  9,  1, Binary,  3, "TR9" },
                {  8,  1, Binary,  3, "TR8" },
                {  7,  1, Binary,  3, "TR7" },
                {  6,  1, Binary,  3, "TR6" },
                {  5,  1, Binary,  3, "TR5" },
                {  4,  1, Binary,  3, "TR4" },
                {  3,  1, Binary,  3, "TR3" },
                {  2,  1, Binary,  3, "TR2" },
                {  1,  1, Binary,  3, "TR1" },
                {  0,  1, Binary,  3, "TR0" },
        };
        static const struct bdesc exti_swier[] = {
                { 31, 12, Binary, 12, "res" },
                { 19,  1, Binary,  3, "res" },
                { 18,  1, Binary,  7, "SWIER18" },
                { 17,  1, Binary,  7, "SWIER17" },
                { 16,  1, Binary,  7, "SWIER16" },
                { 15,  1, Binary,  7, "SWIER15" },
                { 14,  1, Binary,  7, "SWIER14" },
                { 13,  1, Binary,  7, "SWIER13" },
                { 12,  1, Binary,  7, "SWIER12" },
                { 11,  1, Binary,  7, "SWIER11" },
                { 10,  1, Binary, -7, "SWIER10" },
                {  9,  1, Binary,  7, "SWIER9" },
                {  8,  1, Binary,  7, "SWIER8" },
                {  7,  1, Binary,  7, "SWIER7" },
                {  6,  1, Binary,  7, "SWIER6" },
                {  5,  1, Binary,  7, "SWIER5" },
                {  4,  1, Binary,  7, "SWIER4" },
                {  3,  1, Binary,  7, "SWIER3" },
                {  2,  1, Binary,  7, "SWIER2" },
                {  1,  1, Binary,  7, "SWIER1" },
                {  0,  1, Binary,  7, "SWIER0" },
        };
        static const struct bdesc exti_pr[] = {
                { 31, 12, Binary, 12, "res" },
                { 19,  1, Binary,  3, "res" },
                { 18,  1, Binary,  4, "PR18" },
                { 17,  1, Binary,  4, "PR17" },
                { 16,  1, Binary,  4, "PR16" },
                { 15,  1, Binary,  4, "PR15" },
                { 14,  1, Binary,  4, "PR14" },
                { 13,  1, Binary,  4, "PR13" },
                { 12,  1, Binary,  4, "PR12" },
                { 11,  1, Binary,  4, "PR11" },
                { 10,  1, Binary,  4, "PR10" },
                {  9,  1, Binary,  3, "PR9" },
                {  8,  1, Binary,  3, "PR8" },
                {  7,  1, Binary,  3, "PR7" },
                {  6,  1, Binary,  3, "PR6" },
                {  5,  1, Binary,  3, "PR5" },
                {  4,  1, Binary,  3, "PR4" },
                {  3,  1, Binary,  3, "PR3" },
                {  2,  1, Binary,  3, "PR2" },
                {  1,  1, Binary,  3, "PR1" },
                {  0,  1, Binary,  3, "PR0" },
        };

        dump_reg(&EXTI_IMR,"EXTI",0,"IMR",exti_imr,21);
        dump_reg(&EXTI_EMR,"EXTI",0,"EMR",exti_imr,21);
        dump_reg(&EXTI_RTSR,"EXTI",0,"RTSR",exti_rtsr,21);
        dump_reg(&EXTI_FTSR,"EXTI",0,"FTSR",exti_rtsr,21);
        dump_reg(&EXTI_SWIER,"EXTI",0,"SWIER",exti_swier,21);
        dump_reg(&EXTI_PR,"EXTI",0,"PR",exti_pr,21);
}

static int
which_device(int low,int high) {
        char ch;
        int dev;

        std_printf("Which device (%d-%d)? ",low,high);
        ch = std_getc();
        std_printf("%c\n",ch);

        if ( ch < '0' || ch > '9' )
                return -1;
        dev = ch & 0x0F;
        if ( dev < low || dev > high )
                return -1;
        return dev;
}

static void
dump_adc(void) {
        static const struct bdesc adc_sr[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 11, Binary, 11, "res" },
                {  4,  1, Binary,  4, "STRT" },
                {  3,  1, Binary,  5, "JSTRT" },
                {  2,  1, Binary,  4, "JEOC" },
                {  1,  1, Binary,  3, "EOC" },
                {  0,  1, Binary,  3, "AWD" },
        };
        static const struct bdesc adc_cr1[] = {
                { 31,  8, Binary,  8, "res" },
                { 23,  1, Binary,  5, "AWDEN" },
                { 22,  1, Binary,  6, "JAWDEN" },
                { 21,  2, Binary,  3, "res" },
                { 19,  4, Binary,  7, "DUALMOD" },
                { 15,  3, Binary,  7, "DISCNUM" },
                { 12,  1, Binary,  7, "JDISCEN" },
                { 11,  1, Binary,  6, "DISCEN" },
                { 10,  1, Binary,  5, "JAUTO" },
                {  9,  1, Binary,  6, "AWDSGL" },
                {  8,  1, Binary,  4, "SCAN" },
                {  7,  1, Binary,  6, "JEOCIE" },
                {  6,  1, Binary,  5, "AWDIE" },
                {  5,  1, Binary,  5, "EOCIE" },
                {  4,  5, Decimal, 5, "AWDCH" },
        };
        static const struct bdesc adc_cr2[] = {
                { 31, 8, Binary,  8, "res" },
                { 23, 1, Binary,  7, "TSVREFE" },
                { 22, 1, Binary,  7, "SWSTART" },
                { 21, 1, Binary,  8, "JSWSTART" },
                { 20, 1, Binary,  7, "EXTTRIG" },
                { 19, 1, Binary,  6, "EXTSEL" },
                { 16, 1, Binary,  3, "res" },
                { 15, 1, Binary,  8, "JEXTTRIG" },
                { 14, 3, Binary,  7, "JEXTSEL" },
                { 11, 1, Binary,  5, "ALIGN" },
                { 10, 2, Binary,  3, "res" },
                {  8, 1, Binary,  3, "DMA" },
                {  7, 4, Binary,  4, "res" },
                {  3, 1, Binary,  6, "RSTCAL" },
                {  2, 1, Binary,  3, "CAL" },
                {  1, 1, Binary,  4, "CONT" },
                {  0, 1, Binary,  4, "ADON" },
        };
        static const struct bdesc adc_smpr1[] = {
                { 31, 8, Binary,  8, "res" },
                { 23, 3, Binary,  5, "SMP17" },
                { 20, 3, Binary,  5, "SMP16" },
                { 17, 3, Binary,  5, "SMP15" },
                { 14, 3, Binary,  5, "SMP14" },
                { 11, 3, Binary,  5, "SMP13" },
                {  8, 3, Binary,  5, "SMP12" },
                {  5, 3, Binary,  5, "SMP11" },
                {  2, 3, Binary,  5, "SMP10" },
        };
        static const struct bdesc adc_smpr2[] = {
                { 31, 2, Binary,  3, "res" },
                { 29, 3, Binary,  4, "SMP9" },
                { 26, 3, Binary,  4, "SMP8" },
                { 23, 3, Binary,  4, "SMP7" },
                { 20, 3, Binary,  4, "SMP6" },
                { 17, 3, Binary,  4, "SMP5" },
                { 14, 3, Binary,  4, "SMP4" },
                { 11, 3, Binary,  4, "SMP3" },
                {  8, 3, Binary,  4, "SMP2" },
                {  5, 3, Binary,  4, "SMP1" },
                {  2, 3, Binary,  4, "SMP0" },
        };
        static const struct bdesc adc_jofrx[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  4, Binary,  4, "res" },
                { 11, 12, Decimal,12, "JOFFSETx" }
        };
        static const struct bdesc adc_htr[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  4, Binary,  4, "res" },
                { 11, 12, Decimal,12, "HT" }
        };
        static const struct bdesc adc_ltr[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  4, Binary,  4, "res" },
                { 11, 12, Decimal,12, "HT" }
        };
        static const struct bdesc adc_sqr1[] = {
                { 31,  8, Binary,  8, "res" },
                { 23,  4, Decimal, 2, "L" },
                { 19,  5, Decimal, 5, "SQ16" },
                { 14,  5, Decimal, 5, "SQ15" },
                {  9,  5, Decimal, 5, "SQ14" },
                {  4,  5, Decimal, 5, "SQ13" },
        };
        static const struct bdesc adc_sqr2[] = {
                { 31,  2, Binary,  3, "res" },
                { 29,  5, Decimal, 5, "SQ12" },
                { 24,  5, Decimal, 5, "SQ11" },
                { 19,  5, Decimal, 5, "SQ10" },
                { 14,  5, Decimal, 5, "SQ9" },
                {  9,  5, Decimal, 5, "SQ8" },
                {  4,  5, Decimal, 5, "SQ7" },
        };
        static const struct bdesc adc_sqr3[] = {
                { 31,  2, Binary,  3, "res" },
                { 29,  5, Decimal, 5, "SQ6" },
                { 24,  5, Decimal, 5, "SQ5" },
                { 19,  5, Decimal, 5, "SQ4" },
                { 14,  5, Decimal, 5, "SQ3" },
                {  9,  5, Decimal, 5, "SQ2" },
                {  4,  5, Decimal, 5, "SQ1" },
        };
        static const struct bdesc adc_jsqr[] = {
                { 31, 10, Binary, 10, "res" },
                { 21,  2, Binary,  2, "JL" },
                { 19,  5, Binary,  5, "JSQ4" },
                { 14,  5, Binary,  5, "JSQ3" },
                {  9,  5, Binary,  5, "JSQ2" },
                {  4,  5, Binary,  5, "JSQ1" },
        };
        static const struct bdesc adc_jdrx[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     5, "JDATA" },
        };
        static const struct bdesc adc_dr[] = {
                { 31, 16, Hex, 8, "ADC2DATA" },
                { 15, 16, Hex, 5, "DATA" },
        };
        static const uint32_t addrs[] = { ADC1, ADC2 };
        int dev = which_device(1,2);
        const uint32_t a = addrs[dev-1];

        if ( dev < 0 )
                return;

        dump_reg(&ADC_SR(a),"ADC",dev,"SR",adc_sr,7);
        dump_reg(&ADC_CR1(a),"ADC",dev,"CR1",adc_cr1,15);
        dump_reg(&ADC_CR2(a),"ADC",dev,"CR2",adc_cr2,17);
        dump_reg(&ADC_SMPR1(a),"ADC",dev,"SMPR1",adc_smpr1,9);
        dump_reg(&ADC_SMPR2(a),"ADC",dev,"SMPR2",adc_smpr2,11);
        dump_reg(&ADC_JOFR1(a),"ADC",dev,"JOFR1",adc_jofrx,3);
        dump_reg(&ADC_JOFR2(a),"ADC",dev,"JOFR2",adc_jofrx,3);
        dump_reg(&ADC_JOFR3(a),"ADC",dev,"JOFR3",adc_jofrx,3);
        dump_reg(&ADC_JOFR4(a),"ADC",dev,"JOFR4",adc_jofrx,3);
        dump_reg(&ADC_HTR(a),"ADC",dev,"HTR",adc_htr,3);
        dump_reg(&ADC_LTR(a),"ADC",dev,"LTR",adc_ltr,3);
        dump_reg(&ADC_SQR1(a),"ADC",dev,"SQR1",adc_sqr1,6);
        dump_reg(&ADC_SQR2(a),"ADC",dev,"SQR2",adc_sqr2,7);
        dump_reg(&ADC_SQR3(a),"ADC",dev,"SQR3",adc_sqr3,7);
        dump_reg(&ADC_JSQR(a),"ADC",dev,"JSQR",adc_jsqr,6);
        dump_reg(&ADC_JDR1(a),"ADC",dev,"JDR1",adc_jdrx,2);
        dump_reg(&ADC_JDR2(a),"ADC",dev,"JDR2",adc_jdrx,2);
        dump_reg(&ADC_JDR3(a),"ADC",dev,"JDR3",adc_jdrx,2);
        dump_reg(&ADC_JDR4(a),"ADC",dev,"JDR4",adc_jdrx,2);
        dump_reg(&ADC_DR(a),"ADC",dev,"DR",adc_dr,2);
}

static void
dump_timer1(void) {
        static const struct bdesc timx_cr1[] = {
                { 31, 22, Binary, 22, "res" },
                {  9,  2, Binary,  3, "CKD" },
                {  7,  1, Binary,  4, "ARPE" },
                {  6,  2, Binary,  3, "CMS" },
                {  4,  1, Binary,  3, "DIR" },
                {  3,  1, Binary,  3, "OPM" },
                {  2,  1, Binary,  3, "URS" },
                {  1,  1, Binary,  3, "CEN" },
        };
        static const struct bdesc timx_cr2[] = {
                { 31, 17, Binary, 17, "res" },
                { 14,  1, Binary,  4, "OIS4" },
                { 13,  1, Binary,  5, "OIS3N" },
                { 12,  1, Binary,  4, "OIS3" },
                { 11,  1, Binary,  5, "OIS2N" },
                { 10,  1, Binary,  4, "OIS2" },
                {  9,  1, Binary,  5, "OIS1N" },
                {  8,  1, Binary,  4, "OIS1" },
                {  7,  1, Binary,  4, "TIS1" },
                {  6,  3, Binary,  3, "MMS" },
                {  3,  1, Binary,  4, "CCDS" },
                {  2,  1, Binary,  4, "CCUS" },
                {  1,  1, Binary,  3, "res" },
                {  0,  1, Binary,  4, "CCPC" },
        };
        static const struct bdesc timx_smcr[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  1, Binary,  3, "ETP" },
                { 14,  1, Binary,  3, "ECE" },
                { 13,  2, Binary,  4, "ETPS" },
                { 11,  4, Binary,  4, "ETF" },
                {  7,  1, Binary,  3, "MSM" },
                {  6,  3, Binary,  3, "TS" },
                {  3,  1, Binary,  3, "res" },
                {  3,  1, Binary,  3, "SMS" },
        };
        static const struct bdesc timx_dier[] = {
                { 31, 17, Binary, 17, "res" },
                { 14,  1, Binary,  3, "TDE" },
                { 13,  1, Binary,  5, "COMDE" },
                { 12,  1, Binary,  5, "CC4DE" },
                { 11,  1, Binary,  5, "CC3DE" },
                { 10,  1, Binary,  5, "CC2DE" },
                {  9,  1, Binary,  5, "CC1DE" },
                {  8,  1, Binary,  3, "UDE" },
                {  7,  1, Binary,  3, "BIE" },
                {  6,  1, Binary,  3, "TIE" },
                {  5,  1, Binary,  5, "COMIE" },
                {  4,  1, Binary,  5, "CC4IE" },
                {  3,  1, Binary,  5, "CC3IE" },
                {  2,  1, Binary,  5, "CC2IE" },
                {  1,  1, Binary,  5, "CC1IE" },
                {  0,  1, Binary,  3, "UIE" },
        };
        static const struct bdesc timx_sr[] = {
                { 31, 19, Binary, 19, "res" },
                { 12,  1, Binary,  5, "CC4OF" },
                { 11,  1, Binary,  5, "CC3OF" },
                { 10,  1, Binary,  5, "CC2OF" },
                {  9,  1, Binary,  5, "CC1OF" },
                {  8,  1, Binary,  3, "res" },
                {  7,  1, Binary,  3, "BIF" },
                {  6,  1, Binary,  3, "TIF" },
                {  5,  1, Binary,  5, "COMIF" },
                {  4,  1, Binary,  5, "CC4IF" },
                {  3,  1, Binary,  5, "CC3IF" },
                {  2,  1, Binary,  5, "CC2IF" },
                {  1,  1, Binary,  5, "CC1IF" },
                {  0,  1, Binary,  3, "UIF" },
        };
        static const struct bdesc timx_ccmr1a[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  1, Binary,  5, "OC2CE" },
                { 14,  3, Binary,  4, "OC2M" },
                { 11,  1, Binary,  5, "OC2PE" },
                { 10,  1, Binary,  5, "OC2FE" },
                {  9,  2, Binary,  4, "CC2S" },
                {  7,  1, Binary,  5, "OC1CE" },
                {  6,  3, Binary,  4, "OC1M" },
                {  3,  1, Binary,  5, "OC1PE" },
                {  2,  1, Binary,  5, "OC1FE" },
                {  1,  2, Binary,  5, "CC1S" },
        };
        static const struct bdesc timx_ccmr1b[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  4, Binary,  4, "IC2F" },
                { 11,  2, Binary,  6, "IC2PSC" },
                {  9,  2, Binary,  4, "CC2S" },
                {  7,  4, Binary,  4, "IC1F" },
                {  3,  2, Binary,  6, "IC1PSC" },
                {  1,  2, Binary,  4, "CC1S" },
        };
        static const struct bdesc timx_ccmr2a[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  1, Binary,  5, "OC4E" },
                { 14,  3, Binary,  4, "OC4M" },
                { 11,  1, Binary,  5, "OC4PE" },
                { 10,  1, Binary,  5, "OC4FE" },
                {  9,  2, Binary,  4, "CC4S" },
                {  7,  1, Binary,  5, "OC3CE" },
                {  6,  3, Binary,  4, "OC3M" },
                {  3,  1, Binary,  5, "OC3PE" },
                {  2,  1, Binary,  5, "OC3FE" },
                {  1,  2, Binary,  5, "CC3S" },
        };
        static const struct bdesc timx_ccmr2b[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  4, Binary,  4, "IC4F" },
                { 11,  2, Binary,  6, "IC4PSC" },
                {  9,  2, Binary,  4, "CC4S" },
                {  7,  4, Binary,  4, "IC3F" },
                {  3,  2, Binary,  6, "IC3PSC" },
                {  1,  2, Binary,  4, "CC3S" },
        };
        static const struct bdesc timx_ccer[] = {
                { 31, 18, Binary, 18, "res" },
                { 13,  1, Binary,  4, "CC4P" },
                { 12,  1, Binary,  4, "CC4E" },
                { 11,  1, Binary,  5, "CC4NP" },
                { 10,  1, Binary,  5, "CC4NE" },
                {  9,  1, Binary,  4, "CC3P" },
                {  8,  1, Binary,  4, "CC3E" },
                {  7,  1, Binary,  5, "CC3NP" },
                {  6,  1, Binary,  5, "CC3NE" },
                {  5,  1, Binary,  4, "CC2P" },
                {  4,  1, Binary,  4, "CC2E" },
                {  3,  1, Binary,  5, "CC2NP" },
                {  2,  1, Binary,  5, "CC2NE" },
                {  1,  1, Binary,  4, "CC1P" },
                {  0,  1, Binary,  4, "CC1E" },
        };
        static const struct bdesc timx_cnt[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     9, "CNT" }
        };
        static const struct bdesc timx_psc[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     9, "PSC" }
        };
        static const struct bdesc timx_arr[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     9, "ARR" }
        };
        static const struct bdesc timx_rcr[] = {
                { 31, 24, Binary, 24, "res" },
                {  7,  8, Hex,     3, "REP" }
        };
        static const struct bdesc timx_ccr1[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     5, "CCR1" }
        };
        static const struct bdesc timx_ccr2[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     5, "CCR2" }
        };
        static const struct bdesc timx_ccr3[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     5, "CCR3" }
        };
        static const struct bdesc timx_ccr4[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     5, "CCR4" }
        };
        static const struct bdesc timx_bdtr[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  1, Binary,  3, "MOE" },
                { 14,  1, Binary,  3, "AOE" },
                { 13,  1, Binary,  3, "BKP" },
                { 12,  1, Binary,  3, "BKE" },
                { 11,  1, Binary,  4, "OSSR" },
                { 10,  1, Binary,  4, "OSSI" },
                {  9,  2, Binary,  4, "LOCK" },
                {  7,  8, Binary,  8, "DTG" },
        };
        static const struct bdesc timx_dcr[] = {
                { 31, 19, Binary, 19, "res" },
                { 12,  5, Decimal, 3, "DBL" },
                {  7,  3, Binary,  3, "res" },
                {  5,  5, Binary,  5, "DBA" },
        };
        static const struct bdesc timx_dmar[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     9, "DMAR" }
        };

        dump_reg(&TIM1_CR1,"TIM",1,"CR1",timx_cr1,8);
        dump_reg(&TIM1_CR2,"TIM",1,"CR2",timx_cr2,14);
        dump_reg(&TIM1_SMCR,"TIM",1,"SMCR",timx_smcr,14);
        dump_reg(&TIM1_DIER,"TIM",1,"DIER",timx_dier,16);
        dump_reg(&TIM1_SR,"TIM",1,"SR",timx_sr,14);
        dump_reg(&TIM1_CCMR1,"TIM",1,"CCMR1 (out)",timx_ccmr1a,11);
        dump_reg(&TIM1_CCMR1,"TIM",1,"CCMR1 (inp)",timx_ccmr1b,7);
        dump_reg(&TIM1_CCMR2,"TIM",1,"CCMR2 (out)",timx_ccmr2a,11);
        dump_reg(&TIM1_CCMR2,"TIM",1,"CCMR2 (inp)",timx_ccmr2b,7);
        dump_reg(&TIM1_CCER,"TIM",1,"CCER",timx_ccer,15);
        dump_reg(&TIM1_CNT,"TIM",1,"CNT",timx_cnt,2);
        dump_reg(&TIM1_PSC,"TIM",1,"PSC",timx_psc,2);
        dump_reg(&TIM1_ARR,"TIM",1,"ARR",timx_arr,2);
        dump_reg(&TIM1_RCR,"TIM",1,"RCR",timx_rcr,2);
        dump_reg(&TIM1_CCR1,"TIM",1,"CCR1",timx_ccr1,2);
        dump_reg(&TIM1_CCR2,"TIM",1,"CCR2",timx_ccr2,2);
        dump_reg(&TIM1_CCR3,"TIM",1,"CCR3",timx_ccr3,2);
        dump_reg(&TIM1_CCR4,"TIM",1,"CCR4",timx_ccr4,2);
        dump_reg(&TIM1_BDTR,"TIM",1,"BDTR",timx_bdtr,9);
        dump_reg(&TIM1_DCR,"TIM",1,"DCR",timx_dcr,9);
        dump_reg(&TIM1_DMAR,"TIM",1,"DMAR",timx_dmar,2);
}

static void
dump_timers24(int dev) {
        static const struct bdesc timx_cr1[] = {
                { 31, 22, Binary, 22, "res" },
                {  9,  2, Binary,  3, "CKD" },
                {  7,  1, Binary,  4, "ARPE" },
                {  6,  2, Binary,  3, "CMS" },
                {  4,  1, Binary,  3, "DIR" },
                {  3,  1, Binary,  3, "OPM" },
                {  2,  1, Binary,  3, "URS" },
                {  1,  1, Binary,  4, "UDIS" },
                {  0,  1, Binary,  3, "CEN" },
        };
        static const struct bdesc timx_cr2[] = {
                { 31, 24, Binary, 24, "res" },
                {  7,  1, Binary,  4, "TIS1" },
                {  6,  3, Binary,  3, "MMS" },
                {  3,  1, Binary,  4, "CCDS" },
                {  2,  3, Binary,  3, "res" },
        };
        static const struct bdesc timx_smcr[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  1, Binary,  4, "ETP" },
                { 14,  1, Binary,  3, "ECE" },
                { 13,  2, Binary,  4, "ETPS" },
                { 11,  4, Binary,  4, "ETF" },
                {  7,  1, Binary,  3, "MSM" },
                {  6,  3, Binary,  3, "TS" },
                {  3,  1, Binary,  3, "res" },
                {  2,  3, Binary,  3, "SMS" },
        };
        static const struct bdesc timx_dier[] = {
                { 31, 17, Binary, 17, "res" },
                { 14,  1, Binary,  3, "TDE" },
                { 13,  1, Binary,  3, "res" },
                { 12,  1, Binary,  5, "CC4DE" },
                { 11,  1, Binary,  5, "CC3DE" },
                { 10,  1, Binary,  5, "CC2DE" },
                {  9,  1, Binary,  5, "CC1DE" },
                {  8,  1, Binary,  3, "UDE" },
                {  7,  1, Binary,  3, "res" },
                {  6,  1, Binary,  3, "TIE" },
                {  5,  1, Binary,  3, "res" },
                {  4,  1, Binary,  5, "CC4IE" },
                {  3,  1, Binary,  5, "CC3IE" },
                {  2,  1, Binary,  5, "CC2IE" },
                {  1,  1, Binary,  5, "CC1IE" },
                {  0,  1, Binary,  3, "UIE" },
        };
        static const struct bdesc timx_sr[] = {
                { 31, 19, Binary, 19, "res" },
                { 12,  1, Binary,  5, "CC4OF" },
                { 11,  1, Binary,  5, "CC3OF" },
                { 10,  1, Binary,  5, "CC2OF" },
                {  9,  1, Binary,  5, "CC1OF" },
                {  8,  2, Binary,  3, "res" },
                {  6,  1, Binary,  3, "TIF" },
                {  5,  1, Binary,  3, "res" },
                {  4,  1, Binary,  5, "CC4IF" },
                {  3,  1, Binary,  5, "CC3IF" },
                {  2,  1, Binary,  5, "CC2IF" },
                {  1,  1, Binary,  5, "CC1IF" },
                {  0,  1, Binary,  3, "UIF" },
        };
        static const struct bdesc timx_ccmr1a[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  1, Binary,  5, "OC2CE" },
                { 14,  3, Binary,  4, "OC2M" },
                { 11,  1, Binary,  5, "OC2PE" },
                { 10,  1, Binary,  5, "OC2FE" },
                {  9,  2, Binary,  4, "CC2S" },
                {  7,  1, Binary,  5, "OC1CE" },
                {  6,  3, Binary,  4, "OC1M" },
                {  3,  1, Binary,  5, "OC1PE" },
                {  2,  1, Binary,  5, "OC1FE" },
                {  1,  2, Binary,  5, "CC1S" },
        };
        static const struct bdesc timx_ccmr1b[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  4, Binary,  4, "IC2F" },
                { 11,  2, Binary,  6, "IC2PSC" },
                {  9,  2, Binary,  4, "CC2S" },
                {  7,  4, Binary,  4, "IC1F" },
                {  3,  2, Binary,  6, "IC1PSC" },
                {  1,  2, Binary,  4, "CC1S" },
        };
        static const struct bdesc timx_ccmr2a[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  1, Binary,  5, "OC4E" },
                { 14,  3, Binary,  4, "OC4M" },
                { 11,  1, Binary,  5, "OC4PE" },
                { 10,  1, Binary,  5, "OC4FE" },
                {  9,  2, Binary,  4, "CC4S" },
                {  7,  1, Binary,  5, "OC3CE" },
                {  6,  3, Binary,  4, "OC3M" },
                {  3,  1, Binary,  5, "OC3PE" },
                {  2,  1, Binary,  5, "OC3FE" },
                {  1,  2, Binary,  5, "CC3S" },
        };
        static const struct bdesc timx_ccmr2b[] = {
                { 31, 16, Binary, 16, "res" },
                { 15,  4, Binary,  4, "IC4F" },
                { 11,  2, Binary,  6, "IC4PSC" },
                {  9,  2, Binary,  4, "CC4S" },
                {  7,  4, Binary,  4, "IC3F" },
                {  3,  2, Binary,  6, "IC3PSC" },
                {  1,  2, Binary,  4, "CC3S" },
        };
        static const struct bdesc timx_ccer[] = {
                { 31, 18, Binary, 18, "res" },
                { 13,  1, Binary,  4, "CC4P" },
                { 12,  1, Binary,  4, "CC4E" },
                { 11,  2, Binary,  3, "res" },
                {  9,  1, Binary,  4, "CC3P" },
                {  8,  1, Binary,  4, "CC3E" },
                {  7,  1, Binary,  5, "CC3NP" },
                {  6,  1, Binary,  5, "CC3NE" },
                {  5,  1, Binary,  4, "CC2P" },
                {  4,  1, Binary,  4, "CC2E" },
                {  3,  2, Binary,  3, "res" },
                {  1,  1, Binary,  4, "CC1P" },
                {  0,  1, Binary,  4, "CC1E" },
        };
        static const struct bdesc timx_cnt[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     9, "CNT" }
        };
        static const struct bdesc timx_psc[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     9, "PSC" }
        };
        static const struct bdesc timx_arr[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     9, "ARR" }
        };
        static const struct bdesc timx_ccr1[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     5, "CCR1" }
        };
        static const struct bdesc timx_ccr2[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     5, "CCR2" }
        };
        static const struct bdesc timx_ccr3[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     5, "CCR3" }
        };
        static const struct bdesc timx_ccr4[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     5, "CCR4" }
        };
        static const struct bdesc timx_dcr[] = {
                { 31, 19, Binary, 19, "res" },
                { 12,  5, Decimal, 3, "DBL" },
                {  7,  3, Binary,  3, "res" },
                {  5,  5, Binary,  5, "DBA" },
        };
        static const struct bdesc timx_dmar[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,     9, "DMAR" }
        };
        static const uint32_t addrs[] = { TIM2, TIM3, TIM4 };
        const uint32_t a = addrs[dev-2];

        dump_reg(&TIM_CR1(a),"TIM",dev,"CR1",timx_cr1,9);
        dump_reg(&TIM_CR2(a),"TIM",dev,"CR2",timx_cr2,5);
        dump_reg(&TIM_SMCR(a),"TIM",dev,"SMCR",timx_smcr,9);
        dump_reg(&TIM_DIER(a),"TIM",dev,"DIER",timx_dier,16);
        dump_reg(&TIM_SR(a),"TIM",dev,"SR",timx_sr,13);
        dump_reg(&TIM_CCMR1(a),"TIM",dev,"CCMR1 (out)",timx_ccmr1a,11);
        dump_reg(&TIM_CCMR1(a),"TIM",dev,"CCMR1 (inp)",timx_ccmr1b,7);
        dump_reg(&TIM_CCMR2(a),"TIM",dev,"CCMR2 (out)",timx_ccmr2a,11);
        dump_reg(&TIM_CCMR2(a),"TIM",dev,"CCMR2 (inp)",timx_ccmr2b,7);
        dump_reg(&TIM_CCER(a),"TIM",dev,"CCER",timx_ccer,13);
        dump_reg(&TIM_CNT(a),"TIM",dev,"CNT",timx_cnt,2);
        dump_reg(&TIM_PSC(a),"TIM",dev,"PSC",timx_psc,2);
        dump_reg(&TIM_ARR(a),"TIM",dev,"ARR",timx_arr,2);
        dump_reg(&TIM_CCR1(a),"TIM",dev,"CCR1",timx_ccr1,2);
        dump_reg(&TIM_CCR2(a),"TIM",dev,"CCR2",timx_ccr2,2);
        dump_reg(&TIM_CCR3(a),"TIM",dev,"CCR3",timx_ccr3,2);
        dump_reg(&TIM_CCR4(a),"TIM",dev,"CCR4",timx_ccr4,2);
        dump_reg(&TIM_DCR(a),"TIM",dev,"DCR",timx_dcr,9);
        dump_reg(&TIM_DMAR(a),"TIM",dev,"DMAR",timx_dmar,2);
}

static void
dump_backup(void) {
        static const struct bdesc bkp_rtccr[] = {
                { 31, 22, Binary, 22, "res" },
                {  9,  1, Binary,  4, "ASOS" },
                {  8,  1, Binary,  4, "ASOE" },
                {  7,  1, Binary,  3, "CCO" },
                {  6,  7, Binary,  7, "CAL" },
        };
        static const struct bdesc bkp_cr[] = {
                { 31, 30, Binary, 30, "res" },
                {  1,  1, Binary,  4, "TPAL" },
                {  0,  1, Binary,  3, "TPE" },
        };
        static const struct bdesc bkp_csr[] = {
                { 31, 21, Binary, 21, "res" },
                {  9,  1, Binary,  3, "TIF" },
                {  8,  1, Binary,  3, "TEF" },
                {  7,  5, Binary,  5, "res" },
                {  2,  1, Binary,  4, "TPIE" },
                {  1,  1, Binary,  3, "CTI" },
                {  0,  1, Binary,  3, "CTE" },
        };
        static volatile uint32_t *addrs[] = {
                &BKP_DR1,&BKP_DR2, &BKP_DR3, &BKP_DR4, &BKP_DR5, &BKP_DR6, &BKP_DR7, &BKP_DR8, &BKP_DR9, &BKP_DR10
        };
        char name[24];
        uint32_t x;

        dump_reg(&BKP_RTCCR,"BKP",0,"RTCCR",bkp_rtccr,5);
        dump_reg(&BKP_CR,"BKP",0,"CR",bkp_cr,3);
        dump_reg(&BKP_CSR,"BKP",0,"CSR",bkp_csr,7);
        std_putc('\n');

        for ( x=1; x<=10; ++x ) {
                mini_snprintf(name,sizeof name,"BKP_DR%u",(unsigned)x);
                dump_reg_simple16(addrs[x-1],name,0,NULL,*addrs[x-1]);
        }
}

static void
dump_timers(void) {
        int dev = which_device(1,4);

        if ( dev < 0 )
                return;

        switch ( dev ) {
        case 1:
                dump_timer1();
                break;
        case 2:
        case 3:
        case 4:
                dump_timers24(dev);
                break;
        }
}

static void
dump_dma(void) {
        static const struct bdesc dma1_isr[] = {
                { 31,  4, Binary,  4, "res" },
                { 27,  1, Binary,  5, "TEIF7" },
                { 26,  1, Binary,  5, "HTIF7" },
                { 25,  1, Binary,  5, "TCIF7" },
                { 24,  1, Binary,  4, "GIF7" },
                { 23,  1, Binary,  5, "TEIF6" },
                { 22,  1, Binary,  5, "HTIF6" },
                { 21,  1, Binary,  5, "TCIF6" },
                { 20,  1, Binary,  4, "GIF6" },
                { 19,  1, Binary,  5, "TEIF5" },
                { 18,  1, Binary,  5, "HTIF5" },
                { 17,  1, Binary,  5, "TCIF5" },
                { 16,  1, Binary,  4, "GIF5" },
                { 15,  1, Binary,  5, "TEIF4" },
                { 14,  1, Binary,  5, "HTIF4" },
                { 13,  1, Binary,  5, "TCIF4" },
                { 12,  1, Binary,  4, "GIF4" },
                { 11,  1, Binary,  5, "TEIF3" },
                { 10,  1, Binary,  5, "HTIF2" },
                {  9,  1, Binary,  5, "TCIF3" },
                {  8,  1, Binary,  4, "GIF3" },
                {  7,  1, Binary,  5, "TEIF2" },
                {  6,  1, Binary,  5, "HTIF2" },
                {  5,  1, Binary,  5, "TCIF2" },
                {  4,  1, Binary,  4, "GIF2" },
                {  3,  1, Binary,  5, "TEIF1" },
                {  2,  1, Binary,  5, "HTIF1" },
                {  1,  1, Binary,  5, "TCIF1" },
                {  0,  1, Binary,  4, "GIF1" },
        };
        static const struct bdesc dma_ccrx[] = {
                { 31, 17, Binary, 17, "res" },
                { 14,  1, Binary,  7, "MEM2MEM" },
                { 13,  2, Binary,  2, "PL" },
                { 11,  2, Binary,  5, "MSIZE" },
                {  9,  2, Binary,  5, "PSIZE" },
                {  7,  1, Binary,  4, "MINC" },
                {  6,  1, Binary,  4, "PINC" },
                {  5,  1, Binary,  4, "CIRC" },
                {  4,  1, Binary,  3, "DIR" },
                {  3,  1, Binary,  4, "TEIE" },
                {  2,  1, Binary,  4, "HTIE" },
                {  1,  1, Binary,  4, "TCIE" },
                {  0,  1, Binary,  2, "EN" },
        };
        static const struct bdesc dma_cndtrx[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Decimal, 5, "NDT" },
        };
        static const struct bdesc dma_cparx[] = {
                { 31, 32, Hex, 32, "PA" },
        };
        static const struct bdesc dma_cmarx[] = {
                { 31, 32, Hex, 32, "MA" },
        };

        dump_reg(&DMA1_ISR,"DMA",1,"ISR",dma1_isr,29);
        dump_reg(&DMA1_CCR1,"DMA",1,"CCR1",dma_ccrx,13);
        dump_reg(&DMA1_CCR2,"DMA",1,"CCR2",dma_ccrx,13);
        dump_reg(&DMA1_CCR3,"DMA",1,"CCR3",dma_ccrx,13);
        dump_reg(&DMA1_CCR4,"DMA",1,"CCR4",dma_ccrx,13);
        dump_reg(&DMA1_CCR5,"DMA",1,"CCR5",dma_ccrx,13);
        dump_reg(&DMA1_CCR6,"DMA",1,"CCR6",dma_ccrx,13);
        dump_reg(&DMA1_CCR7,"DMA",1,"CCR7",dma_ccrx,13);
        dump_reg(&DMA1_CNDTR1,"DMA",1,"CNDTR1",dma_cndtrx,2);
        dump_reg(&DMA1_CNDTR2,"DMA",1,"CNDTR2",dma_cndtrx,2);
        dump_reg(&DMA1_CNDTR3,"DMA",1,"CNDTR3",dma_cndtrx,2);
        dump_reg(&DMA1_CNDTR4,"DMA",1,"CNDTR4",dma_cndtrx,2);
        dump_reg(&DMA1_CNDTR5,"DMA",1,"CNDTR5",dma_cndtrx,2);
        dump_reg(&DMA1_CNDTR6,"DMA",1,"CNDTR6",dma_cndtrx,2);
        dump_reg(&DMA1_CNDTR7,"DMA",1,"CNDTR7",dma_cndtrx,2);
        dump_reg(&DMA1_CPAR1,"DMA",1,"CPAR1",dma_cparx,1);
        dump_reg(&DMA1_CPAR2,"DMA",1,"CPAR2",dma_cparx,1);
        dump_reg(&DMA1_CPAR3,"DMA",1,"CPAR3",dma_cparx,1);
        dump_reg(&DMA1_CPAR4,"DMA",1,"CPAR4",dma_cparx,1);
        dump_reg(&DMA1_CPAR5,"DMA",1,"CPAR5",dma_cparx,1);
        dump_reg(&DMA1_CPAR6,"DMA",1,"CPAR6",dma_cparx,1);
        dump_reg(&DMA1_CPAR7,"DMA",1,"CPAR7",dma_cparx,1);
        dump_reg(&DMA1_CMAR1,"DMA",1,"CMAR1",dma_cmarx,1);
        dump_reg(&DMA1_CMAR2,"DMA",1,"CMAR2",dma_cmarx,1);
        dump_reg(&DMA1_CMAR3,"DMA",1,"CMAR3",dma_cmarx,1);
        dump_reg(&DMA1_CMAR4,"DMA",1,"CMAR4",dma_cmarx,1);
        dump_reg(&DMA1_CMAR5,"DMA",1,"CMAR5",dma_cmarx,1);
        dump_reg(&DMA1_CMAR6,"DMA",1,"CMAR6",dma_cmarx,1);
        dump_reg(&DMA1_CMAR7,"DMA",1,"CMAR7",dma_cmarx,1);
}

static void
dump_rtc(void) {
        static const struct bdesc rtc_crh[] = {
                { 31, 29, Binary, 29, "res" },
                {  2,  1, Binary,  4, "OWIE" },
                {  1,  1, Binary,  5, "ALRIE" },
                {  0,  1, Binary,  4, "SECIE" },
        };
        static const struct bdesc rtc_crl[] = {
                { 31, 26, Binary, 26, "res" },
                {  5,  1, Binary,  5, "RTOFF" },
                {  4,  1, Binary,  4, "CNF" },
                {  3,  1, Binary,  3, "RSF" },
                {  2,  1, Binary,  3, "OWF" },
                {  1,  1, Binary,  4, "ALRF" },
                {  0,  1, Binary,  4, "SECF" },
        };
        static const struct bdesc rtc_prll[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Decimal, 5, "PRL" },
        };
        static const struct bdesc rtc_divh[] = {
                { 31, 28, Binary, 28, "res" },
                {  3,  4, Hex,     4, "DIVH" },
        };
        static const struct bdesc rtc_divl[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,    16, "DIVL" },
        };
        static const struct bdesc rtc_cnth[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,    16, "CNTH" },
        };
        static const struct bdesc rtc_cntl[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,    16, "CNTL" },
        };
        static const struct bdesc rtc_alrh[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,    16, "ALRH" },
        };
        static const struct bdesc rtc_alrl[] = {
                { 31, 16, Binary, 16, "res" },
                { 15, 16, Hex,    16, "ALRL" },
        };

        dump_reg(&RTC_CRH,"RTC",0,"CRH",rtc_crh,4);
        dump_reg(&RTC_CRL,"RTC",0,"CRL",rtc_crl,7);
        dump_reg(&RTC_PRLL,"RTC",0,"PRLL",rtc_prll,2);
        dump_reg(&RTC_DIVH,"RTC",0,"DIVH",rtc_divh,2);
        dump_reg(&RTC_DIVL,"RTC",0,"DIVL",rtc_divl,2);
        dump_reg(&RTC_CNTH,"RTC",0,"CNTH",rtc_cnth,2);
        dump_reg(&RTC_CNTL,"RTC",0,"CNTL",rtc_cntl,2);
        dump_reg(&RTC_ALRH,"RTC",0,"ALRH",rtc_alrh,2);
        dump_reg(&RTC_ALRL,"RTC",0,"ALRL",rtc_alrl,2);
}

static void
dump_can(void) {
        static const struct bdesc can_mcr[] = {
                { 31, 15, Binary, 15, "res" },
                { 16,  1, Binary,  3, "DBF" },
                { 15,  1, Binary,  5, "RESET" },
                { 14,  7, Binary,  7, "res" },
                {  7,  1, Binary,  4, "TTCN" },
                {  6,  1, Binary,  4, "ABOM" },
                {  5,  1, Binary,  4, "AWUM" },
                {  4,  1, Binary,  4, "NART" },
                {  3,  1, Binary,  4, "RFLM" },
                {  2,  1, Binary,  4, "TXFP" },
                {  1,  1, Binary,  5, "SLEEP" },
                {  0,  1, Binary,  4, "INRQ" }
        };
        static const struct bdesc can_msr[] = {
                { 31, 20, Binary, 20, "res" },
                { 11,  1, Binary,  2, "RX" },
                { 10,  1, Binary,  4, "SAMP" },
                {  9,  1, Binary,  3, "RXM" },
                {  8,  1, Binary,  3, "TXM" },
                {  7,  1, Binary,  3, "res" },
                {  4,  1, Binary,  5, "SLAKI" },
                {  3,  1, Binary,  4, "WKUI" },
                {  2,  1, Binary,  4, "ERRI" },
                {  1,  1, Binary,  4, "SLAK" },
                {  0,  1, Binary,  4, "INAK" }
        };
        static const struct bdesc can_tsr[] = {
                { 31,  1, Binary,  4, "LOW2" },
                { 30,  1, Binary,  4, "LOW1" },
                { 29,  1, Binary,  4, "LOW0" },
                { 28,  1, Binary,  4, "TME2" },
                { 27,  1, Binary,  4, "TME1" },
                { 26,  1, Binary,  4, "TME0" },
                { 25,  2, Binary,  4, "CODE" },
                { 23,  1, Binary,  5, "ABRQ2" },
                { 22,  3, Binary,  3, "res" },
                { 19,  1, Binary,  5, "TERR2" },
                { 18,  1, Binary,  5, "ALST2" },
                { 17,  1, Binary,  5, "TXOK2" },
                { 16,  1, Binary,  5, "RQCP2" },
                { 15,  1, Binary,  5, "ABRQ1" },
                { 14,  3, Binary,  3, "res" },
                { 11,  1, Binary,  5, "TERR1" },
                { 10,  1, Binary,  5, "ALST1" },
                {  9,  1, Binary,  5, "TXOK1" },
                {  8,  1, Binary,  5, "RQCP1" },
                {  7,  1, Binary,  5, "ABRQ0" },
                {  3,  1, Binary,  5, "TERR0" },
                {  2,  1, Binary,  5, "ALST0" },
                {  1,  1, Binary,  5, "TXOK0" },
                {  0,  1, Binary,  5, "RQCP0" },
        };
        static const struct bdesc can_rf0r[] = {
                { 31, 26, Binary, 26, "res" },
                {  5,  1, Binary,  5, "RFMO0" },
                {  4,  1, Binary,  5, "FOVR0" },
                {  3,  1, Binary,  5, "FULL0" },
                {  2,  1, Binary,  3, "res" },
                {  1,  2, Decimal, 4, "FMP0" },
        };
        static const struct bdesc can_rf1r[] = {
                { 31, 26, Binary, 26, "res" },
                {  5,  1, Binary,  5, "RFMO1" },
                {  4,  1, Binary,  5, "FOVR1" },
                {  3,  1, Binary,  5, "FULL1" },
                {  2,  1, Binary,  3, "res" },
                {  1,  2, Decimal, 4, "FMP1" },
        };
        static const struct bdesc can_ier[] = {
                { 31, 14, Binary, 14, "res" },
                { 17,  1, Binary,  5, "SLKIE" },
                { 16,  1, Binary,  5, "WKUIE" },
                { 15,  1, Binary,  5, "ERRIE" },
                { 14,  3, Binary,  3, "res" },
                { 11,  1, Binary,  5, "LECIE" },
                { 10,  1, Binary,  5, "BOFIE" },
                {  9,  1, Binary,  5, "EPVIE" },
                {  8,  1, Binary,  5, "EWGIE" },
                {  7,  1, Binary,  3, "res" },
                {  6,  1, Binary,  6, "FOVIE1" },
                {  5,  1, Binary,  5, "FFIE1" },
                {  4,  1, Binary,  6, "FMPIE1" },
                {  3,  1, Binary,  6, "FOVIE0" },
                {  2,  1, Binary,  5, "FFIE0" },
                {  1,  1, Binary,  6, "FMPIE0" },
                {  0,  1, Binary,  5, "TMEIE" },
        };
        static const struct bdesc can_esr[] = {
                { 31,  8, Decimal, 5, "REC" },
                { 22,  8, Decimal, 5, "TEC" },
                { 15,  9, Binary,  9, "res" },
                {  6,  3, Binary,  3, "LEC" },
                {  3,  1, Binary,  3, "res" },
                {  2,  1, Binary,  4, "BOFF" },
                {  1,  1, Binary,  4, "EPVF" },
                {  0,  1, Binary,  4, "EWGF" },
        };
        static const struct bdesc can_btr[] = {
                { 31,  1, Binary,  4, "SILM" },
                { 30,  1, Binary,  4, "LBKM" },
                { 29,  4, Binary,  4, "res" },
                { 25,  2, Binary,  3, "SWJ" },
                { 23,  1, Binary,  3, "res" },
                { 22,  3, Decimal, 3, "TS2" },
                { 19,  4, Decimal, 4, "TS1" },
                { 15,  6, Binary,  6, "res" },
                {  9, 10, Decimal, 4, "BRP" },
        };
        static const struct bdesc can_tixr[] = {
                { 31, 11, Hex,     5, "STDID" },
                { 20, 18, Hex,     6, "EXID" },
                {  2,  1, Binary,  3, "IDE" },
                {  1,  1, Binary,  3, "RTR" },
                {  0,  1, Binary,  4, "TXRQ" },
        };
        static const struct bdesc can_tdtxr[] = {
                { 31, 16, Decimal, 5, "TIME" },
                { 15,  7, Binary,  7, "res" },
                {  8,  1, Binary,  3, "TGT" },
                {  7,  4, Binary,  4, "res" },
                {  3,  4, Decimal, 3, "DCL" },
        };
        static const struct bdesc can_tdlxr[] = {
                { 31,  8, Binary,  8, "DATA3" },
                { 23,  8, Binary,  8, "DATA2" },
                { 15,  8, Binary,  8, "DATA1" },
                {  7,  8, Binary,  8, "DATA0" },
        };
        static const struct bdesc can_tdhxr[] = {
                { 31,  8, Binary,  8, "DATA7" },
                { 23,  8, Binary,  8, "DATA6" },
                { 15,  8, Binary,  8, "DATA5" },
                {  7,  8, Binary,  8, "DATA4" },
        };
        static const struct bdesc can_rixr[] = {
                { 31, 11, Hex,     5, "STDID" },
                { 20, 18, Hex,     6, "EXID" },
                {  2,  1, Binary,  3, "IDE" },
                {  1,  1, Binary,  3, "RTR" },
                {  0,  1, Binary,  3, "res" },
        };
        static const struct bdesc can_rdtxr[] = {
                { 31, 16, Decimal, 5, "TIME" },
                { 15,  8, Binary,  8, "FMI" },
                {  7,  4, Binary,  4, "res" },
                {  3,  4, Decimal, 3, "DCL" },
        };
        
        dump_reg(&CAN_MCR(CAN1),"CAN",0,"MCR",can_mcr,12);
        dump_reg(&CAN_MSR(CAN1),"CAN",0,"MSR",can_msr,11);
        dump_reg(&CAN_TSR(CAN1),"CAN",0,"TSR",can_tsr,24);
        dump_reg(&CAN_ESR(CAN1),"CAN",0,"ESR",can_esr,8);
        dump_reg(&CAN_BTR(CAN1),"CAN",0,"BTR",can_btr,9);
        dump_reg(&CAN_RF0R(CAN1),"CAN",0,"RF0R",can_rf0r,6);
        dump_reg(&CAN_RF1R(CAN1),"CAN",0,"RF1R",can_rf1r,6);
        dump_reg(&CAN_IER(CAN1),"CAN",0,"IER",can_ier,17);
        dump_reg(&CAN_TI0R(CAN1),"CAN",0,"TI0R",can_tixr,5);
        dump_reg(&CAN_TI1R(CAN1),"CAN",0,"TI1R",can_tixr,5);
        dump_reg(&CAN_TI2R(CAN1),"CAN",0,"TI2R",can_tixr,5);
        dump_reg(&CAN_TDT0R(CAN1),"CAN",0,"TDT0R",can_tdtxr,5);
        dump_reg(&CAN_TDT1R(CAN1),"CAN",0,"TDT1R",can_tdtxr,5);
        dump_reg(&CAN_TDT2R(CAN1),"CAN",0,"TDT2R",can_tdtxr,5);
        dump_reg(&CAN_TDL0R(CAN1),"CAN",0,"TDL0R",can_tdlxr,4);
        dump_reg(&CAN_TDL1R(CAN1),"CAN",0,"TDL1R",can_tdlxr,4);
        dump_reg(&CAN_TDL2R(CAN1),"CAN",0,"TDL2R",can_tdlxr,4);
        dump_reg(&CAN_TDH0R(CAN1),"CAN",0,"TDH0R",can_tdhxr,4);
        dump_reg(&CAN_TDH1R(CAN1),"CAN",0,"TDH1R",can_tdhxr,4);
        dump_reg(&CAN_TDH2R(CAN1),"CAN",0,"TDH2R",can_tdhxr,4);
        dump_reg(&CAN_RI0R(CAN1),"CAN",0,"RI0R",can_rixr,5);
        dump_reg(&CAN_RI1R(CAN1),"CAN",0,"RI1R",can_rixr,5);
        dump_reg(&CAN_RDT0R(CAN1),"CAN",0,"RDT0R",can_rdtxr,4);
        dump_reg(&CAN_RDT1R(CAN1),"CAN",0,"RDT1R",can_rdtxr,4);
        dump_reg(&CAN_RDL0R(CAN1),"CAN",0,"RDL0R",can_tdlxr,4);
        dump_reg(&CAN_RDL1R(CAN1),"CAN",0,"RDL1R",can_tdlxr,4);
        dump_reg(&CAN_RDH0R(CAN1),"CAN",0,"RDH0R",can_tdhxr,4);
        dump_reg(&CAN_RDH1R(CAN1),"CAN",0,"RDH1R",can_tdhxr,4);
}

static void
dump_can_filter(void) {
        static const struct bdesc can_fmr[] = {
                { 31, 18, Binary, 18, "res" },
                { 13,  8, Decimal, 6, "CAN2SB" },
                {  7,  7, Binary,  7, "res" },
                {  0,  1, Binary,  5, "FINIT" },
        };
        static const struct bdesc can_fm1r[] = {
                { 31,  4, Binary,  4, "res" },
                { 27,  1, Binary,  5, "FBM27" },
                { 26,  1, Binary,  5, "FBM26" },
                { 25,  1, Binary,  5, "FBM25" },
                { 24,  1, Binary,  5, "FBM24" },
                { 23,  1, Binary,  5, "FBM23" },
                { 22,  1, Binary,  5, "FBM22" },
                { 21,  1, Binary,  5, "FBM21" },
                { 20,  1, Binary,  5, "FBM20" },
                { 19,  1, Binary,  5, "FBM19" },
                { 18,  1, Binary,  5, "FBM18" },
                { 17,  1, Binary,  5, "FBM17" },
                { 16,  1, Binary,  5, "FBM16" },
                { 15,  1, Binary,  -5, "FBM15" },
                { 14,  1, Binary,  5, "FBM14" },
                { 13,  1, Binary,  5, "FBM13" },
                { 12,  1, Binary,  5, "FBM12" },
                { 11,  1, Binary,  5, "FBM11" },
                { 10,  1, Binary,  5, "FBM10" },
                {  9,  1, Binary,  4, "FBM9" },
                {  8,  1, Binary,  4, "FBM8" },
                {  7,  1, Binary,  4, "FBM7" },
                {  6,  1, Binary,  4, "FBM6" },
                {  5,  1, Binary,  4, "FBM5" },
                {  4,  1, Binary,  4, "FBM4" },
                {  3,  1, Binary,  4, "FBM3" },
                {  2,  1, Binary,  4, "FBM2" },
                {  1,  1, Binary,  4, "FBM1" },
                {  0,  1, Binary,  4, "FBM0" },
        };
        static const struct bdesc can_fs1r[] = {
                { 31,  4, Binary,  4, "res" },
                { 27,  1, Binary,  5, "FSC27" },
                { 26,  1, Binary,  5, "FSC26" },
                { 25,  1, Binary,  5, "FSC25" },
                { 24,  1, Binary,  5, "FSC24" },
                { 23,  1, Binary,  5, "FSC23" },
                { 22,  1, Binary,  5, "FSC22" },
                { 21,  1, Binary,  5, "FSC21" },
                { 20,  1, Binary,  5, "FSC20" },
                { 19,  1, Binary,  5, "FSC19" },
                { 18,  1, Binary,  5, "FSC18" },
                { 17,  1, Binary,  5, "FSC17" },
                { 16,  1, Binary,  5, "FSC16" },
                { 15,  1, Binary,  -5, "FSC15" },
                { 14,  1, Binary,  5, "FSC14" },
                { 13,  1, Binary,  5, "FSC13" },
                { 12,  1, Binary,  5, "FSC12" },
                { 11,  1, Binary,  5, "FSC11" },
                { 10,  1, Binary,  5, "FSC10" },
                {  9,  1, Binary,  4, "FSC9" },
                {  8,  1, Binary,  4, "FSC8" },
                {  7,  1, Binary,  4, "FSC7" },
                {  6,  1, Binary,  4, "FSC6" },
                {  5,  1, Binary,  4, "FSC5" },
                {  4,  1, Binary,  4, "FSC4" },
                {  3,  1, Binary,  4, "FSC3" },
                {  2,  1, Binary,  4, "FSC2" },
                {  1,  1, Binary,  4, "FSC1" },
                {  0,  1, Binary,  4, "FSC0" },
        };
        // CAN_FFA1R
        // CAN_FA1R
        // CAN_F0R1
        // CAN_F0R2
        // ...
        // CAN_F13R1
        // CAN_F13R2
        
        dump_reg(&CAN_FMR(CAN1),"CAN",0,"FMR",can_fmr,4);
        dump_reg(&CAN_FM1R(CAN1),"CAN",0,"FM1R",can_fm1r,29);
        dump_reg(&CAN_FS1R(CAN1),"CAN",0,"FS1R",can_fs1r,29);

}

/*********************************************************************
 * Monitor routine
 *********************************************************************/

void
monitor(void) {
        int ch;
        bool menuf = true;
        
        for (;;) {
                if ( menuf )
                        std_printf(
                                "\nSTM32F103C8T6 Menu:\n"
                                "  a ... ADC Registers\n"
                                "  b ... Backupe Registers\n"
                                "  d ... DMA Registers\n"
                                "  f ... AFIO Registers\n"
                                "  k ... CAN Registers\n"
                                "  q ... CAN Filter Registers\n"
                                "  r ... RCC Registers\n"
                                "  t ... Timer Registers\n"
                                "  u ... RTC Registers\n"
                                "  v ... Interrupt Registers\n"
                                "\n"
                                "  i ... GPIO Inputs\n"
                                "  o ... GPIO Outputs\n"
                                "  l ... GPIO Lock\n"
                                "  g ... GPIO Config/Mode Registers\n"
                                "\n"
                                "  x ... Exit\n"
                        );
                menuf = false;

                std_printf("\n: ");
                ch = std_getc();

                if ( isalpha(ch) )
                        ch = toupper(ch);
                std_printf("%c\n",ch);

                switch ( ch ) {
                case '?':
                case '\r':
                case '\n':
                        menuf = true;
                        break;          
                case 'A':
                        dump_adc();
                        break;
                case 'B':
                        dump_backup();
                        break;
                case 'D':
                        dump_dma();
                        break;
                case 'F':
                        dump_afio();
                        break;
                case 'G' :
                        dump_gpio();
                        break;
                case 'I':
                        dump_gpio_inputs();
                        break;
                case 'K':
                        dump_can();
                        break;
                case 'L':
                        dump_gpio_locks();
                        break;
                case 'O':
                        dump_gpio_outputs();
                        break;
                case 'Q':
                        dump_can_filter();
                        break;
                case 'R':
                        dump_rcc();
                        break;
                case 'T':
                        dump_timers();
                        break;
                case 'U':
                        dump_rtc();
                        break;
                case 'V':
                        dump_intr();
                        break;
                case 'X':
                        return;
                default:
                        std_printf(" ???\n");
                        menuf = true;
                }
        }
}

// monitor.c
