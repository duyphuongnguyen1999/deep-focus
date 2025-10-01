#ifndef DHT11_READER_H
#define DHT11_READER_H

#define DHT_TYPE DHT_TYPE_DHT11
#define DHT_GPIO GPIO_NUM_4

#define TAG "DHT11_READER"

void read_dht_task(void *pvParameter);

#endif // DHT11_READER_H