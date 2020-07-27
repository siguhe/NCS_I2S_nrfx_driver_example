#include <zephyr.h>
#include <string.h>

#include "nrfx_i2s.h"

//#error Remember to define these pins before you run! // This assume you only have these 3 pins for I2S.
#define I2S_WS_PIN 4
#define I2S_SD_PIN 5
#define I2S_SCK_PIN 6

#define I2S_DATA_BLOCK_WORDS 512 //How many numbers do we want. time reorded = DATA_BLOCK_WORDS / freq

static u32_t m_buffer_rx32u[I2S_DATA_BLOCK_WORDS];
static s32_t tmp[I2S_DATA_BLOCK_WORDS];
static nrfx_i2s_buffers_t initial_buffers;
static bool data_ready_flag = false;

ISR_DIRECT_DECLARE(i2s_isr_handler)
{
	data_ready_flag = false;
	nrfx_i2s_irq_handler();
	ISR_DIRECT_PM(); /* PM done after servicing interrupt for best latency
			  */
	return 1;		 /* We should check if scheduling decision should be made */
}

static void data_handler(nrfx_i2s_buffers_t const *p_released, u32_t status)
{
	if (NRFX_I2S_STATUS_NEXT_BUFFERS_NEEDED == status)
	{
		nrfx_err_t err = nrfx_i2s_next_buffers_set(&initial_buffers);
		if (err != NRFX_SUCCESS)
		{
			printk("Error!, continuing running as if nothing happened, but you should probably investigate.\n");
		}
	}
	if (p_released)
	{
		if (p_released->p_rx_buffer != NULL)
		{
			data_ready_flag = true; //This is used in print_sound()
		}
	}
}

void print_sound()
{
	while (!data_ready_flag)
	{
		k_sleep(K_MSEC(1));
		//Wait for data. Since we do not want I2S_DATA_BLOCK_WORDS amount of prints inside the interrupt.
	}
	nrfx_i2s_stop();
	data_ready_flag = false;

	/** Print the raw data from the I2S microphone */
	/*
	for (int i = 0; i < I2S_DATA_BLOCK_WORDS; i++)
	{
		printk("%u, ", m_buffer_rx32u[i]); //The audio is automatically saved in m_buffer_rx32u by the interrupt
		k_sleep(K_MSEC(16));
	}
	printk("\n\n");
	*/

	/**  Format the data from Adafruit I2S MEMS Microphone Breakout, then print it	*/

	s64_t sum = 0;
	s64_t words = I2S_DATA_BLOCK_WORDS;

	for (int i = 0; i < I2S_DATA_BLOCK_WORDS; i++)
	{
		memcpy(tmp + i, m_buffer_rx32u + i, sizeof(u32_t));
		tmp[i] >>= 8;
		sum += tmp[i];
	}

	s64_t mean = sum / words;
	for (int i = 0; i < I2S_DATA_BLOCK_WORDS; i++)
	{
		tmp[i] -= mean;
		printk("%d, ", tmp[i]);
		k_sleep(K_MSEC(16));
	}
	printk("\n\n");
	/**  End of formatted data print*/
}

nrfx_err_t get_sound_init()
{
	IRQ_DIRECT_CONNECT(I2S0_IRQn, 0, i2s_isr_handler, 0);
	memset(&m_buffer_rx32u, 0x00, sizeof(m_buffer_rx32u));
	initial_buffers.p_rx_buffer = m_buffer_rx32u;

	//YOu should probably change this config to fit your I2S microphone and audio preferences.
	nrfx_i2s_config_t config =
		NRFX_I2S_DEFAULT_CONFIG(I2S_SCK_PIN, I2S_WS_PIN,
								NRFX_I2S_PIN_NOT_USED,
								NRFX_I2S_PIN_NOT_USED, I2S_SD_PIN);

	config.mode = NRF_I2S_MODE_MASTER;			//Microphone requirement
	config.ratio = NRF_I2S_RATIO_64X;			//Microphone requirement
	config.sample_width = NRF_I2S_SWIDTH_24BIT_IN32BIT; //Microphone requirement
	config.mck_setup = NRF_I2S_MCK_32MDIV31;	//Preference     freq = (MCKfreq/ratio) =16.129 KHz.
	config.channels = NRF_I2S_CHANNELS_LEFT;	//Preference

	nrfx_err_t err_code = nrfx_i2s_init(&config, data_handler);
	if (err_code != NRFX_SUCCESS)
	{
		printk("I2S init error\n");
		return err_code;
	}
	err_code = nrfx_i2s_start(&initial_buffers, I2S_DATA_BLOCK_WORDS, 0); //start recording
	if (err_code != NRFX_SUCCESS)
	{
		printk("I2S start error\n");
		return err_code;
	}
	//nrfx_i2s_stop() //stop recording

	k_sleep(K_SECONDS(2));
	return err_code;
}

void main()
{
	nrfx_err_t err;
	err = get_sound_init();
	if (err != NRFX_SUCCESS)
	{
		return;
	}
	while (1)
	{
		k_sleep(K_SECONDS(5));
		print_sound();
	}
}
