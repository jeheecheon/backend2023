#!/usr/bin/python3

from flask import Flask

app = Flask(__name__)

@app.route('/')
def hello_world():
    return 'Hello, World!' + 1

@app.route('/bad', methods=['GET', 'POST'])
def bad_world():
    return 'Bad World!'

@app.route('/good')
def good_world():
    return 'Good World!'

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=19135)
