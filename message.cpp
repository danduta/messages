#include "include/message.h"
#include "include/client.h"

#include <bits/stdc++.h>

#include <iostream>
#include <string>
#include <math.h>

std::string type_to_string(const message& m)
{
    switch (m.udp_msg.type) {
    case INT:
        return "INT";
    case SHORT_REAL:
        return "SHORT_REAL";
    case FLOAT:
        return "FLOAT";
    case STRING:
        return "STRING";
    default:
        return "UNKNOWN_TYPE";
    }
}

std::string payload_to_string(message& m)
{
    char* payload = m.udp_msg.payload;
    char sign;

    if (*payload == POS) {
        sign = '+';
    } else if (*payload == NEG) {
        sign = '-';
    }

    switch (m.udp_msg.type)
    {
    case INT:
        return sign + std::to_string(*(uint32_t*)(payload + 1));
    case SHORT_REAL:
        double result;
        result = ((*(uint16_t*)(payload)) / (double)100);
        return std::to_string(result);
    case FLOAT:
        double number;
        uint32_t abs;
        uint8_t negative_power;
        
        abs = *(uint32_t*)(payload + sizeof(uint8_t));
        negative_power = *(uint8_t*)(payload + sizeof(uint8_t) + sizeof(uint32_t));
        number = (abs / (pow(10, negative_power)));

        return sign + std::to_string(number);
    case STRING:
        payload[PAYLOAD_LEN] = '\0';

        return std::string(payload);
    default:
        return "UNKNOWN_TYPE";
    }
}

std::ostream& operator<<(std::ostream& os, message& m)
{
    char topic[TOPIC_LEN + 1];
    strncpy(topic, m.udp_msg.topic, TOPIC_LEN);
    topic[TOPIC_LEN] = '\0';
    
    os << inet_ntoa(m.addr.sin_addr) << ':' << ntohs(m.addr.sin_port) <<
        " - " << topic <<
        " - " << type_to_string(m) <<
        " - " << payload_to_string(m);

    return os;
}