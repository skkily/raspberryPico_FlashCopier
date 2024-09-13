/*
 * Flash copier, used to copy data from source flash to target flash.
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <rtdbg.h>
#include <spi_flash_sfud.h>

/*
 * LED_PIN: Specifies the LED pin, which is used to indicate the status
 *               OFF: not working ON: working FLASHING: error 
 * SPI0_CS: Specify the CS pin of the target copy flash
 * SPI1_CS: Specify the CS pin of the source copy flash
 * BTN_IN & BTN_OUT: Connect a button to execute the copy procedure
 */

#define LED_PIN 25
#define SPI0_CS 6
#define SPI1_CS 11
#define BTN_OUT 17
#define BTN_IN 18

/*
 * rwsize: Block size for read, write and erase, 
 * spi0_devcie: Data copy target device
 * spi1_device: Data copy source device
 * data_buffer: R/W data buffer, size is related to rwsize
 */

static uint32_t rwsize = 4 * 1024;

static struct rt_spi_device *spi0_device = RT_NULL;
static struct rt_spi_device *spi1_device = RT_NULL;
static void *data_buffer = RT_NULL;


/*
 * Copy flash data
 * According to the rwsize size,
 * first check whether the data is consistent,
 * then decide whether to copy it.
 * Finally, check all the data again.
 * 
 * cp_addr: The address of the data to be copied
 * cp_size: The size of the data to be copied.
 * Both must be aligned to rwsize
 * 
 */

bool copy_flash(uint32_t cp_addr, uint32_t cp_size)
{
    static rt_spi_flash_device_t rtt_dev0 = NULL;
    static rt_spi_flash_device_t rtt_dev1 = NULL;

	if ( 0 != cp_addr % rwsize || 0 != cp_size % rwsize)
	{
	    LOG_E("Not byte aligned\n");
	    return false;
	}

    if (rtt_dev0)
	    rt_sfud_flash_delete(rtt_dev0);
    if (rtt_dev1)
	    rt_sfud_flash_delete(rtt_dev1);

    rtt_dev0 = rt_sfud_flash_probe("flash0", "spi00");
    if (!rtt_dev0)
    {
	    LOG_E("probe flash0 err\n");
	    return false;
    }
    rtt_dev1 = rt_sfud_flash_probe("flash1", "spi10");
    if (!rtt_dev1)
    {
        LOG_E("probe flash1 err\n");
	    return false;
    }
    sfud_flash_t sfud_dev0 = (sfud_flash_t)rtt_dev0->user_data;
    sfud_flash_t sfud_dev1 = (sfud_flash_t)rtt_dev1->user_data;
	uint32_t size_dev0 = sfud_dev0->chip.capacity;
	uint32_t size_dev1 = sfud_dev1->chip.capacity;

	if (0 == cp_size && 0 == cp_addr)
	{
		cp_addr = 0;
		cp_size = size_dev1;
	}

	uint32_t stop_offset = cp_addr + cp_size;
    if (stop_offset > size_dev0 || stop_offset > size_dev1)
    {
    	LOG_E("Copy data overflow\n");
	    return false;
    }

	for (uint32_t offset = cp_addr; offset < stop_offset; offset += rwsize)
	{
		if ( 0 ==  offset % (128 * 1024))
    		LOG_D("sfud process 0x%x \n", offset);

		if(SFUD_SUCCESS != sfud_read(sfud_dev1, offset, rwsize, data_buffer))
		{
    		LOG_E("sfud read err\n");
	    	return false;
		}
		if(SFUD_SUCCESS != sfud_read(sfud_dev0, offset, rwsize, ((char *)data_buffer) + rwsize))
		{
    		LOG_E("sfud read err\n");
	    	return false;
		}
		if(SFUD_SUCCESS == rt_memcmp(data_buffer, ((char *)data_buffer) + rwsize, rwsize))
		{
			continue;
		}
		if(SFUD_SUCCESS != sfud_erase(sfud_dev0, offset, rwsize))
		{
    		LOG_E("sfud erase err\n");
	    	return false;
		}
		if(SFUD_SUCCESS != sfud_write(sfud_dev0, offset, rwsize, data_buffer))
		{
    		LOG_E("sfud write err\n");
	    	return false;
		}
	}

   	LOG_I("sfud checking\n");
	for (uint32_t offset = cp_addr; offset < stop_offset; offset += rwsize)
	{
		if(SFUD_SUCCESS != sfud_read(sfud_dev1, offset, rwsize, data_buffer))
		{
   			LOG_E("sfud read err\n");
	    	return false;
		}
		if(SFUD_SUCCESS != sfud_read(sfud_dev0, offset, rwsize, ((char *)data_buffer) + rwsize))
		{
   			LOG_E("sfud write err\n");
	    	return false;
		}
		if(SFUD_SUCCESS != rt_memcmp(data_buffer, ((char *)data_buffer) + rwsize, rwsize))
		{
   			LOG_E("sfud check err\n");
	    	return false;
		}
	}
   	LOG_I("sfud check ok\n");
	return true;
}

/* Register a terminal command */
void cpf(void)
{
	copy_flash(0, 0);
}
MSH_CMD_EXPORT(cpf , fengcmd: copy flash);

/* Monitor button actions, execute copy program, and display status on LED light */
int main(void)
{
	bool result = true;

    rt_pin_mode(LED_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LED_PIN, 0);

    rt_pin_mode(BTN_IN, PIN_MODE_INPUT);
    rt_pin_mode(BTN_OUT, PIN_MODE_OUTPUT);
    rt_pin_write(BTN_OUT, 1);

    int count = 0;
    while (1)
    {
    	if (PIN_HIGH == rt_pin_read(17))
		{
		    rt_pin_write(LED_PIN, 1);
			result = copy_flash(0x100000, 0x200000);
		}
		if (!result)
		{
			count++;
			if (50 < count)
			{
    			rt_pin_write(LED_PIN, 1);
			} else {
    			rt_pin_write(LED_PIN, 0);
			}
			if (100 <= count)
				count = 0;
		}
		else rt_pin_write(LED_PIN, 0);

    	rt_thread_mdelay(10);
    }
}

/* Apply for required memory in advance and register spi device */
static int copy_flash_preinit(void)
{
    spi0_device = (struct rt_spi_device *)rt_malloc(sizeof(struct rt_spi_device));
    if(RT_NULL == spi0_device)
    {
        LOG_E("Failed to malloc the spi device.");
        return -RT_ERROR;
    }

    spi1_device = (struct rt_spi_device *)rt_malloc(sizeof(struct rt_spi_device));
    if(RT_NULL == spi1_device)
    {
        LOG_E("Failed to malloc the spi device.");
        return -RT_ERROR;
    }

    data_buffer = rt_malloc(rwsize * 2);
    if(RT_NULL == data_buffer)
    {
        LOG_E("Failed to malloc the data buffer.");
        return -RT_ERROR;
    }

    if (RT_EOK != rt_spi_bus_attach_device_cspin(spi0_device, "spi00", "spi0", SPI0_CS, RT_NULL))
    {
        LOG_E("Failed to attach the spi00 device.");
        return -RT_ERROR;
    }

    if (RT_EOK != rt_spi_bus_attach_device_cspin(spi1_device, "spi10", "spi1", SPI1_CS, RT_NULL))
    {
        LOG_E("Failed to attach the spi10 device.");
        return -RT_ERROR;
    }

}
INIT_COMPONENT_EXPORT(copy_flash_preinit);

