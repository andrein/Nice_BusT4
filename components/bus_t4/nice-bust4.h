/*
  Nice BusT4
  Обмен данными по UART на скорости 19200 8n1
  Перед пакетом с данными отправляется break длительностью 519us (10 бит)
  Содержимое пакета, которое удалось понять, описано в структуре packet_cmd_body_t

 

  Для Oview к адресу всегда прибавляется 80.
  Адрес контроллера ворот без изменений.


Подключение

BusT4                       ESP8266

Стенка устройства        Rx Tx GND
9  7  5  3  1  
10 8  6  4  2
место для кабеля
            1 ---------- Rx
            2 ---------- GND
            4 ---------- Tx
            5 ---------- +24V




Из мануала nice_dmbm_integration_protocol.pdf

• ADR: это адрес сети NICE, где находятся устройства, которыми вы хотите управлять. Это может быть значение от 1 до 63 (от 1 до 3F).
Это значение должно быть в HEX. Если адресатом является модуль интеграции на DIN-BAR, это значение равно 0 (adr = 0), если адресат
является интеллектуальным двигателем, это значение равно 1 (adr = 1).
• EPT: это адрес двигателя Nice, входящего в сетевой ADR. Это может быть значение от 1 до 127. Это значение должно быть в HEX.
• CMD: это команда, которую вы хотите отправить по назначению (ADR, EPT).
• PRF: команда установки профиля.
• FNC: это функция, которую вы хотите отправить по назначению (ADR, EPT).
• EVT: это событие, которое отправляется в пункт назначения (ADR, EPT).



*/


#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"           // для добавления Action
#include "esphome/components/cover/cover.h"
#include <HardwareSerial.h>
#include "esphome/core/helpers.h"              // парсим строки встроенными инструментами
#include <queue>                               // для работы с очередью



namespace esphome {
namespace bus_t4 {

/* для короткого обращения к членам класса */
using namespace esphome::cover;
//using esp8266::timeoutTemplate::oneShotMs;


static const int _UART_NO=UART0; /* номер uart */
static const int TX_P = 1;         /* пин Tx */
static const uint32_t BAUD_BREAK = 9200; /* бодрэйт для длинного импульса перед пакетом */
static const uint32_t BAUD_WORK = 19200; /* рабочий бодрэйт */
static const uint8_t START_CODE = 0x55; /*стартовый байт пакета */


/* сетевые настройки esp
  Ряд может принимать значения от 0 до 63, по-умолчанию 0
  Адрес OVIEW начинается с 8

  При объединении в сеть несколько приводов с OXI необходимо для разных приводов указать разные ряды.
  В этом случае У OXI ряд должен быть как у привода, которым он управляет.
*/






/* Группа сообщения пакетов
  пока нас интересует только CMD,
  остальные глубоко не изучал и номера не проверял
*/
enum mes_type : uint8_t {
  CMD = 0x01,  /* номер проверен, отправка команд автоматике */
//  LSC = 0x02,  /* работа со списками сценариев */
//  LST = 0x03,  /* работа со списками автоматик */
//  POS = 0x04,  /* запрос и изменение позиции автоматики */
//  GRP = 0x05,  /* отправка команд группе автоматик с указанием битовой маски мотора */
//  SCN = 0x06,  /* работа со сценариями */
//  GRC = 0x07,  /* отправка команд группе автоматик, созданных через Nice Screen Configuration Tool */
  INF = 0x08,  /* возвращает или устанавливает информацию об устройстве */
//  LGR = 0x09,  /* работа со списками групп */
//  CGR = 0x0A,  /* работа с категориями групп, созданных через Nice Screen Configuration Tool */
};




/* меню команды в иерархии oview*/
enum cmd_mnu  : uint8_t {
  ROOT = 0x00,
  CONTROL = 0x01,
  SETUP  = 0x04,
};

/* в сочетании с командой определяет тип отправляемого сообщения */
enum inf_cmd  : uint8_t { //cmd_submnu
  TYPE_M = 0x00,   // Запрос типа привода
  INF_STATUS = 0x01, //	Состояние ворот (Открыто/Закрыто/Остановлено)	
  MAC = 0x07,    // mac address.
  MAN = 0x08,   // manufacturer.
  PRD = 0x09,   // product.
  HWR = 0x0a,   // hardware version.
  FRM = 0x0b,   // firmware version.
  DSC = 0x0c,   // description.
  CUR_POS = 0x11,  // текущее условное положение автоматики 
  MAX_OPN = 0x12,   // Максимально возможное открывание по энкодеру.
  POS_MAX = 0x18,   // Максимальное положение (открывания) по энкодеру
  POS_MIN = 0x19,   // Минимальное положение (закрывания) по энкодеру	
  INF_P_OPN1 = 0x21, //	Частичное открывание1 
  INF_P_OPN2 = 0x22, //	Частичное открывание2
  INF_P_OPN3 = 0x23, //	Частичное открывание3
  INF_SLOW_OPN = 0x24, // Замедление в открывании
  INF_SLOW_CLS = 0x25, // Замедление в закрывании	
  INF_IO = 0xD1,    //	состояние входов-выходов

  CUR_MAN = 0x02,  // Текущий Маневр
  SUBMNU  = 0x04,  // Подменю
  STA = 0x40,   // статус в движении
  MAIN_SET = 0x80,   // Основные параметры
  RUN = 0x82,   // Команда для выполнения
	
	
};


/* Команда, которая должна быть выполнена.   Используется в запросах и ответах */
enum control_cmd : uint8_t { 
  SBS = 0x01,    /* Step by Step */
  STOP = 0x02,   /* Stop */
  OPEN = 0x03,   /* Open */
  CLOSE = 0x04,  /* Close */
  P_OPN1 = 0x05, /* Partial opening 1 - частичное открывание, режим калитки */
  P_OPN2 = 0x06, /* Partial opening 2 */
  P_OPN3 = 0x07, /* Partial opening 3 */
  RSP = 0x19, /* ответ интерфейса, подтверждающий получение команды  */
  EVT = 0x29, /* ответ интерфейса, отправляющий запрошенную информацию */
 
  P_OPN4 = 0x0b, /* Partial opening 4 - Коллективно */
  P_OPN5 = 0x0c, /* Partial opening 5 - Приоритет пошагово */
  P_OPN6 = 0x0d, /* Partial opening 6 - Открыть и блокировать */
  UNLK_OPN = 0x19, /* Разблокировать и открыть */
  CLS_LOCK = 0x0E, /* Закрыть и блокировать */
  UNLCK_CLS = 0x1A, /*  Разблокировать и Закрыть */
  LOCK = 0x0F, /* Блокировать*/
  UNLOCK = 0x10, /* Разблокировать */
  LIGHT_TIMER = 0x11, /* Таймер освещения */
  LIGHT_SW = 0x12, /* Освещение вкл/выкл */
  HOST_SBS = 0x13, /* Ведущий SBS */
  HOST_OPN = 0x14, /* Ведущий открыть */
  HOST_CLS = 0x15, /* Ведущий закрыть */
  SLAVE_SBS = 0x16, /*  Ведомый SBS */
  SLAVE_OPN = 0x17, /* Ведомый открыть */
  SLAVE_CLS = 0x18, /* Ведомый закрыть */
  AUTO_ON = 0x1B, /* Автооткрывание активно */
  AUTO_OFF = 0x1C, /* Автооткрывание неактивно	  */

	 
  
};

/* run cmd */
enum run_cmd  : uint8_t {
  SET = 0xA9,  /* запрос на изменение параметров */
  GET = 0x99,   /* запрос на получение параметров */
  GET_SUPP_CMD = 0x89, /* получить поддерживаемые команды */
  };
	
	

/* используется в ответах STA*/
enum Status : uint8_t {
  OPENING  = 0x83,
  CLOSING  = 0x84,
  OPENING2  = 0x02,
  CLOSING2  = 0x03,	
  OPENED   = 0x04,
  CLOSED   = 0x05,
  STOPPED   = 0x88,
  UNKNOWN   = 0x00,
  UNLOCKED = 0x02,
  NO_LIM   = 0x06, /* no limits set */ 
  NO_INF   = 0x0F, /* no additional information */
};


/* Ошибки */
enum errors_byte  : uint8_t {
  NOERR = 0x00, // Нет ошибок
  FD = 0x01,    // Нет команды для этого устройства
  };

// Типы моторов
enum motor_type  : uint8_t {
  SLIDING = 0x01, 
  SECTIONAL = 0x02,
  SWING = 0x03,
  BARRIER = 0x04,
  UPANDOVER = 0x05, // up-and-over подъемно-поворотные ворота
  };


/* Информация для лучшего понимания состава пакетов в протоколе */
// тело пакета запроса CMD
// пакеты с размером тела 0x0c=12 байт 
	/*
struct packet_cmd_body_t {
  uint8_t byte_55;              // Заголовок, всегда 0x55
  uint8_t pct_size1;                // размер тела пакета (без заголовка и CRC. Общее количество  байт минус три), для команд = 0x0c
  uint8_t for_series;           // серия кому пакет ff = всем
  uint8_t for_address;          // адрес кому пакет ff = всем
  uint8_t from_series;           // серия от кого пакет
  uint8_t from_address;          // адрес от кого пакет
  uint8_t mes_type;           // тип сообщения, 1 = CMD, 8 = INF
  uint8_t mes_size;              // количество байт дальше за вычетом двух байт CRC в конце, для команд = 5
  uint8_t crc1;                // CRC1, XOR шести предыдущих байт
  uint8_t cmd_mnu;                // Меню команды. cmd_mnu = 1 для команд управления
  uint8_t cmd_submnu;            // Подменю, в сочетании с группой команды определяет тип отправляемого сообщения
  uint8_t run_cmd;            // Команда, которая должна быть выполнена
  uint8_t offset;            //  Смещение для ответов. Влияет на запросы вроде списка поддерживаемых комманд
  uint8_t crc2;            // crc2, XOR четырех предыдущих байт
  uint8_t pct_size2;            // размер тела пакета (без заголовка и CRC. Общее количество  байт минус три), для команд = 0x0c

};





// тело INF пакета запроса GET/SET
// пакеты с размером тела 0x0e=14 байт и больше
struct packet_get_set_body_t {
  uint8_t byte_55;              // Заголовок, всегда 0x55
  uint8_t pct_size1;                // размер тела пакета (без заголовка и CRC. Общее количество  байт минус три), >= 0x0e
  uint8_t to_series;           // серия кому пакет ff = всем
  uint8_t to_address;          // адрес кому пакет ff = всем
  uint8_t from_series;           // серия от кого пакет
  uint8_t from_address;          // адрес от кого пакет
  uint8_t mes_type;           // тип сообщения, для этих пакетов всегда  8 = INF
  uint8_t mes_size;              // количество байт дальше за вычетом двух байт CRC в конце, для команд = 5
  uint8_t crc1;                // CRC1, XOR шести предыдущих байт
  uint8_t cmd_mnu;                // Меню команды. cmd_mnu = 1 для команд управления
  uint8_t inf_cmd;            // Подменю, в сочетании с группой команды определяет тип отправляемого сообщения
  uint8_t run_cmd;            // Команда, которая должна быть выполнена. Для SET = A9, для GET = 99
  uint8_t offset;               // Смещение для ответов. Влияет на запросы вроде списка поддерживаемых комманд
  uint8_t data_size;            // Размер блока данных
  uint8_t data_blk;            // Блок данных, может занимать несколько байт
  uint8_t crc2;            // crc2, XOR четырех предыдущих байт
  uint8_t pct_size2;            // размер тела пакета (без заголовка и CRC. Общее количество  байт минус три), >= 0x0e

};

// тело пакета ответа RSP
// пакеты с размером тела 0x0e=14 байт 
struct packet_rsp_body_t {
  uint8_t byte_55;              // Заголовок, всегда 0x55
  uint8_t pct_size1;                // размер тела пакета (без заголовка и CRC. Общее количество  байт минус три), >= 0x0e
  uint8_t to_series;           // серия кому пакет ff = всем
  uint8_t to_address;          // адрес кому пакет ff = всем
  uint8_t from_series;           // серия от кого пакет
  uint8_t from_address;          // адрес от кого пакет
  uint8_t mes_type;           // тип сообщения, для этих пакетов всегда  8 = INF
  uint8_t mes_size;              // количество байт дальше за вычетом двух байт CRC в конце, для команд = 5
  uint8_t crc1;                // CRC1, XOR шести предыдущих байт
  uint8_t cmd_mnu;                // Меню команды. cmd_mnu = 1 для команд управления
  uint8_t sub_inf_cmd;            // Из какого подменю получил команду. Значение меньше на 0x80, чем первоначальное подменю
  uint8_t sub_run_cmd;            // Какую команду получил. Значение больше на 0x80, чем полученная команда
  uint8_t hb_data;             // данные, старший бит
  uint8_t lb_data;            // данные, младший бит
  uint8_t err;               // Ошибки
  uint8_t crc2;            // crc2, XOR четырех предыдущих байт
  uint8_t pct_size2;            // размер тела пакета (без заголовка и CRC. Общее количество  байт минус три), >= 0x0e

};
	
 // тело пакета ответа с данными EVT
 
 struct packet_evt_body_t {
  uint8_t byte_55;              // Заголовок, всегда 0x55
  uint8_t pct_size1;                // размер тела пакета (без заголовка и CRC. Общее количество  байт минус три), >= 0x0e
  uint8_t to_series;           // серия кому пакет ff = всем
  uint8_t to_address;          // адрес кому пакет ff = всем
  uint8_t from_series;           // серия от кого пакет
  uint8_t from_address;          // адрес от кого пакет
  uint8_t mes_type;           // тип сообщения, для этих пакетов всегда  8 = INF
  uint8_t mes_size;              // количество байт дальше за вычетом двух байт CRC в конце, для команд = 5
  uint8_t crc1;                // CRC1, XOR шести предыдущих байт
  uint8_t cmd_mnu;                // девятый байт, меню команды, на которую отвечаем. Значение равно первоначальному меню
  uint8_t sub_inf_cmd;            // Из какого подменю получил команду. Значение равно первоначальному подменю
  uint8_t sub_run_cmd;            // На какую команду отвечаем. Значение меньше на 0x80, чем отправленная ранее команда
  uint8_t data_size;            // Размер блока данных
  uint8_t err;               // Ошибки
  uint8_t data_blk;            // Блок данных, может занимать несколько байт
  uint8_t crc2;            // crc2, XOR всех предыдущих байт до девятого
  uint8_t pct_size2;            // размер тела пакета (без заголовка и CRC. Общее количество  байт минус три), >= 0x0e

};
 
 
*/




// создаю класс, наследую членов классов Component и Cover
class NiceBusT4 : public Component, public Cover {
  public:
    void setup() override;
    void loop() override;
    void dump_config() override; // для вывода в лог информации об оборудовнии
//    void send_open();
    void send_raw_cmd(std::string data);
    void set_class_gate(uint8_t class_gate) { class_gate_ = class_gate; }


    void set_to_address(uint16_t to_address) {this->to_addr = to_address;}
    void set_from_address(uint16_t from_address) {this->from_addr = from_address;} 	
//      uint8_t start_code = START_CODE;
//      uint8_t address_h = (uint8_t)(to_address >> 8);
//      uint8_t address_l = (uint8_t)(to_address & 0xFF);
//      this->header_ = {&start_code, &address_h, &address_l};
    
    void set_update_interval(uint32_t update_interval) {  // интервал получения статуса привода
      this->update_interval_ = update_interval;
    }
    //  void on_rs485_data(const std::vector<uint8_t> &data) override;
    cover::CoverTraits get_traits() override;

  protected:
    void control(const cover::CoverCall &call) override;
    void send_command_(const uint8_t *data, uint8_t len);


    uint32_t update_interval_{500};
    uint32_t last_update_{0};
    uint32_t last_uart_byte_{0};
  //  uint8_t current_request_{GET_STATUS}; // осталось от dooya, возможно придется переписать согласно статусам от nice
    uint8_t last_published_op_;
    float last_published_pos_;
    
    uint8_t class_gate_ = 0x55; // 0x01 sliding, 0x02 sectional, 0x03 swing, 0x04 barrier, 0x05 up-and-over
    uint8_t last_init_command_;
    // переменные для uart
    uint8_t _uart_nr;
    uart_t* _uart = nullptr;
    uint16_t _max_opn = 0;  // максимальная позиция энкодера или таймера
    uint16_t _pos_opn = 0;  // позиция открытия энкодера или таймера, не для всех приводов	
    uint16_t _pos_cls = 0;  // позиция закрытия энкодера или таймера, не для всех приводов
    uint16_t _pos_usl = 0;  // условная текущая позиция энкодера или таймера, не для всех приводов	
    // настройки заголовка формируемого пакета
    uint16_t from_addr = 0x0066; //от кого пакет, адрес bust4 шлюза
    uint16_t to_addr = 0x00ff;	 // кому пакет, адрес контроллера привода, которым управляем

	
	
    std::vector<uint8_t> raw_cmd_prepare (std::string data);             // подготовка введенных пользователем данных для возможности отправки
    	
	
   // void send_array_cmd (std::vector<char> data);
    void send_array_cmd (std::vector<uint8_t> data);	
    void send_array_cmd (const uint8_t *data, size_t len);

	
    void parse_status_packet (const std::vector<uint8_t> &data); // разбираем пакет статуса
    
    void handle_char_(uint8_t c);                                         // обработчик полученного байта
    void handle_datapoint_(const uint8_t *buffer, size_t len);          // обработчик полученных данных
    bool validate_message_();                                         // функция проверки полученного сообщения

    std::vector<uint8_t> rx_message_;                          // здесь побайтно накапливается принятое сообщение
    std::queue<std::vector<uint8_t>> tx_buffer_;             // очередь команд для отправки
    bool ready_to_tx_{true};	                           // флаг возможности отправлять команды
	
    std::vector<uint8_t> manufacturer_;
    std::vector<uint8_t> product_;
    std::vector<uint8_t> hardware_;
    std::vector<uint8_t> firmware_;
	
    std::vector<uint8_t> gen_control_cmd(const uint8_t control_cmd);
    std::vector<uint8_t> gen_inf_cmd(const uint8_t cmd_mnu, const uint8_t inf_cmd, const uint8_t run_cmd, const std::vector<uint8_t> &data, size_t len);	
    std::vector<uint8_t> gen_inf_cmd(const uint8_t cmd_mnu, const uint8_t inf_cmd, const uint8_t run_cmd) {return gen_inf_cmd(cmd_mnu, inf_cmd, run_cmd, {0x00}, 0 );} // для команд без данных

}; //класс

} // namespace bus_t4
} // namespace esphome
