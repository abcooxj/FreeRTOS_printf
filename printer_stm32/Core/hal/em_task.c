#include "em_task.h"
#include "em_button.h"
#include "em_motor.h"
#include "em_printer.h"
#include "em_adc.h"

Button_t Button;
bool printer_test = false;

static int digitalRead(int pin){
	//printf("-%d---\n",HAL_GPIO_ReadPin(BTN_GPIO_Port,BTN_Pin));
	return HAL_GPIO_ReadPin(BTN_GPIO_Port,BTN_Pin);
}

void Btn_Dowm_CallBack(void *btn)
{
    printf("Button 单击!\n");
    printer_test = true;
    // read_all_hal();
}

void Btn_Double_CallBack(void *btn)
{
    printf("Button 双击!\n");
}

void Btn_Long_CallBack(void *btn)
{
    printf("Button 长按!\n");
    device_state_t *pdevice = get_device_state();
    bool need_beep = false;
    // 不缺纸且不在打印中才执行
    if (pdevice->paper_state == PAPER_STATUS_NORMAL)
    {
        if (pdevice->printer_state == PRINTER_STATUS_FINISH ||
            pdevice->printer_state == PRINTER_STATUS_INIT)
        {
            printf("开始走纸\n");
            motor_start();
        }
        else
        {
            need_beep = true;
        }
    }
    else
        need_beep = true;
    if (need_beep)
    {
        run_beep(BEEP_WARN);
				run_led(LED_WARN);
    }
}

void Btn_Logn_Free_CallBack(void *btn)
{
    printf("停止走纸\n");
    motor_stop();
}

uint8_t Read_Button_Level(void)
{
    return digitalRead(PIN_KEY);
}

void init_key()
{
    Button_Create("Button",                                           // 按键名字
                  &Button,                                            // 按键句柄
                  Read_Button_Level,                                  // 按键电平检测函数接口
                  LOW);                                               // 触发电平
    Button_Attach(&Button, BUTTON_DOWM, Btn_Dowm_CallBack);           // 按键单击
    Button_Attach(&Button, BUTTON_DOUBLE, Btn_Double_CallBack);       // 双击
    Button_Attach(&Button, BUTTON_LONG, Btn_Long_CallBack);           // 长按
    Button_Attach(&Button, BUTTON_LONG_FREE, Btn_Logn_Free_CallBack); // 长按释放
}

/**
 * @brief 处理上报相关事件
 *
 */
void run_report()
{
    if (get_state_timeout())
    {
				clean_state_timeout();
        read_all_hal();
        if (get_ble_connect())
        {
						printf("report device status:report time up\n");
            ble_report();
        }
    }
    if (read_paper_irq_need_report_status())
    {
        // 缺纸中断产生，需要上报
        printf("report device status : paper irq\n");
        ble_report();
    }
}

/**
 * @brief 处理打印相关事件
 *
 */
void run_printer()
{
    device_state_t *pdevice = get_device_state();
		#ifdef START_PRINTER_WHEN_FINISH_RAED
        if (pdevice->read_ble_finish == true)
        {
            if (pdevice->printer_state == PRINTER_STATUS_FINISH ||
                pdevice->printer_state == PRINTER_STATUS_INIT)
            {
                pdevice->read_ble_finish = false;
                pdevice->printer_state = PRINTER_STATUS_START;
                ble_report();
                printf("report device status : printing start %d\n",get_ble_rx_leftline());
                run_beep(BEEP_PRINTER_START);
                run_led(LED_PRINTER_START);
            }
        }
    #else
			// 接收大于100条时，才触发开始打印
			if (get_ble_rx_leftline()> 100)
			{
					if (pdevice->printer_state == PRINTER_STATUS_FINISH ||
							pdevice->printer_state == PRINTER_STATUS_INIT)
					{
							pdevice->printer_state = PRINTER_STATUS_START;
							ble_report();
							printf("report device status : printing start\n");
							run_beep(BEEP_PRINTER_START);
							run_led(LED_PRINTER_START);
					}
			}
		#endif
    // 开始打印
    if (pdevice->printer_state == PRINTER_STATUS_START)
    {
        // 正常打印
        start_printing_by_queuebuf();
        pdevice->printer_state = PRINTER_STATUS_FINISH;
    }
}

void task_report(void *pvParameters)
{
		int count = 0;
		printf("task_report init\n");
    for (;;) // A Task shall never return or exit.
    {
        run_report();
        vTaskDelay(100);
				count ++;
				if(count >= 50){
					count = 0;
					printf("task_report run\n");
				}
    }
}

void task_button(void *pvParameters)
{
		int count = 0;
		printf("task_button init\n");
    for (;;) // A Task shall never return or exit.
    {
        Button_Process(); // 需要周期调用按键处理函数
        vTaskDelay(20);
				count ++;
				if(count >= 250){
					count = 0;
					printf("task_button run\n");
				}
    }
}

void task_printer(void *pvParameters)
{
		int count = 0;
		init_ble();
		printf("task_printer init\n");
    for (;;) // A Task shall never return or exit.
    {
				ble_status_data_clean();
        run_printer();
        vTaskDelay(1);
        if(printer_test){
            printer_test = false;
            testSTB();
        }
				count ++;
				if(count >= 5000){
					count = 0;
					printf("task_printer run\n");
				}
    }
}

//void printer_run(){
//    // printer_test = true;
//    for (;;) // A Task shall never return or exit.
//    {
//        run_printer();
//        // vTaskDelay(10);
//        if(printer_test){
//            printer_test = false;
//            testSTB();
//        }
//    }
//}

void init_task()
{
    printf("init_task\n");
    init_device_state();
    init_timer();
    init_hal();
    init_queue();
    adc_init();
    init_key();
    init_printer();
    xTaskCreate(
        task_report,  // 任务函数
        "TaskReport", // 任务名
        128,         // 任务栈
        NULL,         // 任务参数
        1,            // 任务优先级, with 3 (configMAX_PRIORITIES - 1) 是最高的，0是最低的.
        NULL          // 任务句柄
    );

    xTaskCreate(
        task_button,  // 任务函数
        "TaskButton", // 任务名
        128,         // 任务栈
        NULL,         // 任务参数
        0,            // 任务优先级, with 3 (configMAX_PRIORITIES - 1) 是最高的，0是最低的.
        NULL          // 任务句柄
    );

    xTaskCreate(
        task_printer,  // 任务函数
        "TaskPrinter", // 任务名
        256,          // 任务栈
        NULL,          // 任务参数
        2,             // 任务优先级, with 3 (configMAX_PRIORITIES - 1) 是最高的，0是最低的.
        NULL           // 任务句柄
    );
}


