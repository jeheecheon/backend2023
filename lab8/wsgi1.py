def application(environ, start_response):
    print(environ['REQUEST_METHOD'])
    print(environ['PATH_INFO'])

    status = '200 OK'
    headers = [('Content-Type', 'text/html')]

    start_response(status, headers)

    return [b'Hello World']

# uwsgi --socket 127.0.0.1:19135 --plugin /usr/lib/uwsgi/plugins/python3_plugin.so --module wsgi1
# # uwsgi --ini uwsgi.ini --module wsgi1