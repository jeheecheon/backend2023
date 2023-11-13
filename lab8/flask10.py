#!/usr/bin/python3

from flask import Flask, request, make_response
from http import HTTPStatus
app = Flask(__name__)

# 인자 형식에 맞게 입력된지 확인
def check_arguments(arg1, op, arg2):
    if arg1 is None or op is None or arg2 is None:
        return True
    
    if type(arg1) is not int or type(arg2) is not int or type(op) is not str:
        return True
    
    if op not in ["+", "-", "*"]:
        return True
    
    try:
        # 문자열인 경우 처리
        arg1 = int(arg1)
        arg2 = int(arg2)
    except ValueError:
        return True
    
    return False

# 계산
def perform_operation(arg1, op, arg2):
    if op == "+":
        return arg1 + arg2
    elif op == "-":
        return arg1 - arg2
    else:
        return arg1 * arg2

# /숫자1/연산자/숫자2 로 입력받아 계산
# curl -X GET http://localhost:19135/12/+/1
@app.route('/<int:arg1>/<string:op>/<int:arg2>', methods=['GET'])
def calculate_get(arg1, op, arg2):
    if check_arguments(arg1, op, arg2):
        return make_response('잘못된 입력...', HTTPStatus.BAD_REQUEST)
    
    return make_response(str(perform_operation(arg1, op, arg2)), HTTPStatus.OK)

# json으로 인자들을 받아 계산
# curl -X POST -H "Content-Type: application/json" -d '{"arg1": 10, "op": "*", "arg2": 11}' http://localhost:19135/
@app.route('/', methods=['POST'])
def calculate_post():
    arg1 = request.get_json().get('arg1', 'No key')
    op = request.get_json().get('op', 'No key')
    arg2 = request.get_json().get('arg2', 'No key')

    print( arg2)
    if check_arguments(arg1, op, arg2):
        return make_response('잘못된 입력...', HTTPStatus.BAD_REQUEST)

    return make_response(str(perform_operation(arg1, op, arg2)), HTTPStatus.OK)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=19135)
