import json

def application(environ, start_response):
    print(environ['REQUEST_METHOD'])
    print(environ['PATH_INFO'])

    body_bytes = environ['wsgi.input'].read()
    body_str = body_bytes.decode('utf-8')
    body_json = json.loads(body_str)

    status = '200 OK'
    headers = [('Content-Type', 'text/html')]

    start_response(status, headers)

    response = f'Hello World {body_json["name"]}'
    return [bytes(response, encoding='utf-8')]


# uwsgi --ini uwsgi.ini --module wsgi2
# uwsgi --socket 127.0.0.1:19135 --plugin /usr/lib/uwsgi/plugins/python3_plugin.so --module wsgi2

# curl -X POST http://localhost/60182228 -H "Content-Type: application/json; charset=utf-8" --data '{"name": "Jehee CHeon"}'