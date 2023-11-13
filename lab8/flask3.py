#!/usr/bin/python3

from flask import Flask

app = Flask(__name__)

@app.route('/<greeting>/<name>')
def greet(greeting, name):
    return f'{greeting}, {name}!'

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=19135)
