# 윗 부분에서 socket 패키지를 import 한다.
import socket
import json 
import sys 

def main(argv):
    obj1 = { 
    'name': 'DK Moon',
    'id': 12345678, 
    'work': {
        'name': 'Myongji Unviversity', 
        'address': '116 Myongji-ro' 
        } 
    }
    s = json.dumps(obj1) 

    # 윗 부분에서 socket 객체를 만들고 데이터를 전송한다.
    # 이 때 C 에서와 마찬가지로 보낼 데이터를 주소로 전달해야된다.   
    # bytes() 는 객체 형태의 문자열을 메모리에 씌여지는 byte 형태로 변경하는 함수라고 생각하면 된다.
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
    sock.sendto(bytes(s, encoding='utf-8'), ('127.0.0.1', 10001))
    (data, sender) = sock.recvfrom(65536)

    obj2 = json.loads(data) 
    print(obj2['name'], obj2['id'], obj2['work']['address']) 
    print(obj1 == obj2)

if __name__ == '__main__': 
    main(sys.argv)