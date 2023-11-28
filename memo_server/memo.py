from http import HTTPStatus
import random
import requests
import urllib
import mysql.connector as database

from flask import abort, Flask, make_response, render_template, Response, redirect, request

app = Flask(__name__)

naver_client_id = 'Z0Taie9OYDDditUhF4GC' # Client ID
naver_client_secret = 'WRGUdnVAQE' # Client Secret
naver_user_url = 'https://openapi.naver.com/v1/nid/me' # 회원 프로필 요청 url
naver_token_url = 'https://nid.naver.com/oauth2.0/token' # 토큰 요청 url
naver_redirect_uri = 'http://60182228-lb-166353545.ap-northeast-2.elb.amazonaws.com/memo/naver-oauth' # redirect url

connection = database.connect(
    user='root',
    password='ghkfud',
    host='172.31.1.135',
    database='memo_db')

@app.route('/')
def home():
    # 쿠기를 통해 이전에 로그인 한 적이 있는지를 확인한다.
    # 이 부분이 동작하기 위해서는 OAuth 에서 access token 을 얻어낸 뒤
    # user profile REST api 를 통해 유저 정보를 얻어낸 뒤 'userId' 라는 cookie 를 지정해야 된다.
    # (참고: 아래 onOAuthAuthorizationCodeRedirected() 마지막 부분 response.set_cookie('userId', user_id) 참고)
    userId = request.cookies.get('userId', default=None)
    name = None

    ####################################################
    # userId 로부터 DB 에서 사용자 이름을 얻어오는 코드를 여기에 작성해야 함
    if userId is not None and userId is not '':
        with connection.cursor() as cur:
            try:
                cur.execute("SELECT USER_NAME FROM APP_USER WHERE USER_ID = %s", (userId,))
                result = cur.fetchone()
                name = result[0] if result else None
            except Exception as e:
                print(f"ERROR during FETCH-ing user name: {e}")
                raise
    ####################################################

    # 이제 클라에게 전송해 줄 index.html 을 생성한다.
    # template 로부터 받아와서 name 변수 값만 교체해준다.
    return render_template('index.html', name=name)


# 로그인 버튼을 누른 경우 이 API 를 호출한다.
# OAuth flow 상 브라우저에서 해당 URL 을 바로 호출할 수도 있으나,
# 브라우저가 CORS (Cross-origin Resource Sharing) 제약 때문에 HTML 을 받아온 서버가 아닌 곳에
# HTTP request 를 보낼 수 없는 경우가 있다. (예: 크롬 브라우저)
# 이를 우회하기 위해서 브라우저가 호출할 URL 을 HTML 에 하드코딩하지 않고,
# 아래처럼 서버가 주는 URL 로 redirect 하는 것으로 처리한다.
#
# 주의! 아래 API 는 잘 동작하기 때문에 손대지 말 것
@app.route('/login')
def onLogin():
    params={
            'response_type': 'code',
            'client_id': naver_client_id,
            'redirect_uri': naver_redirect_uri,
            'state': random.randint(0, 10000)
        }
    urlencoded = urllib.parse.urlencode(params)
    url = f'https://nid.naver.com/oauth2.0/authorize?{urlencoded}'
    return redirect(url)


# 아래는 Redirect URI 로 등록된 경우 호출된다.
# 만일 본인의 Redirect URI 가 http://localhost:8000/auth 의 경우처럼 /auth 대신 다른 것을
# 사용한다면 아래 @app.route('/auth') 의 내용을 그 URL 로 바꿀 것
@app.route('/naver-oauth')
def onOAuthAuthorizationCodeRedirected():
    # 1. redirect uri 를 호출한 request 로부터 authorization code 와 state 정보를 얻어낸다.
    authorization_code = request.args.get('code')
    state = request.args.get('state')

    # 2. authorization code 로부터 access token 을 얻어내는 네이버 API 를 호출한다.
    url_encoded = urllib.parse.urlencode({
        'grant_type': 'authorization_code',
        'client_id': naver_client_id,
        'client_secret': naver_client_secret,
        'code': authorization_code,
        'state': state
    })
    token_response = requests.post(url = f'{naver_token_url}?{url_encoded}')
    json_response = token_response.json();
    access_token = json_response.get('access_token');

    # 3. 얻어낸 access token 을 이용해서 프로필 정보를 반환하는 API 를 호출하고,
    #    유저의 고유 식별 번호를 얻어낸다.
    user_response = requests.get(
        url = naver_user_url, 
        headers = {
            'Authorization': f'Bearer {access_token}'
        }
    )
    user_response_json = user_response.json()
    response_json = user_response_json.get('response')
    user_id = response_json.get('id')
    user_name = response_json.get('name')

    # 4. 얻어낸 user id 와 name 을 DB 에 저장한다.
    with connection.cursor() as cur:
        try:
            cur.execute("INSERT INTO APP_USER (USER_ID, USER_NAME) VALUES (%s, %s)",
                (user_id, user_name))
            connection.commit()
        except Exception as e:
            print(f"ERROR during INSERT: {e}")
            raise

    # 5. 첫 페이지로 redirect 하는데 로그인 쿠키를 설정하고 보내준다.
    response = redirect('/')
    response.set_cookie('userId', user_id)
    return response


@app.route('/memo', methods=['GET'])
def get_memos():
    # 로그인이 안되어 있다면 로그인 하도록 첫 페이지로 redirect 해준다.
    userId = request.cookies.get('userId', default=None)
    if not userId:
        return redirect('/')

    result = []
    with connection.cursor() as cur:
        try:
            cur.execute("select CONTENT from MEMO where USER_ID = %s", (userId,))
            for row in cur.fetchall():
                result.append({
                    'text': row[0]
                })
        except Exception as e:
            print(f"ERROR during FETCH-ing posts: {e}")
            raise

    # memos라는 키 값으로 메모 목록 보내주기
    return {'memos': result}


@app.route('/memo', methods=['POST'])
def post_new_memo():
    # 로그인이 안되어 있다면 로그인 하도록 첫 페이지로 redirect 해준다.
    userId = request.cookies.get('userId', default=None)
    if not userId:
        return redirect('/')

    # 클라이언트로부터 JSON 을 받았어야 한다.
    if not request.is_json:
        abort(HTTPStatus.BAD_REQUEST)

    # 클라이언트로부터 받은 JSON 에서 메모 내용을 추출한 후 DB에 userId 의 메모로 추가한다.
    text = request.json.get('text')
    print(text)

    with connection.cursor() as cur:
        try:
            cur.execute("INSERT INTO MEMO (USER_ID, CONTENT) VALUES (%s, %s)",
                        (userId, text))
            connection.commit()
        except Exception as e:
            print(f"ERROR during INSERT-ing a post: {e}")
            raise
    
    return '', HTTPStatus.OK

@app.route('/ip', methods=['GET'])
def get_public_ip():
    return urllib.request.urlopen("http://169.254.169.254/latest/meta-data/local-ipv4").read()

if __name__ == '__main__':
    app.run('0.0.0.0', port=8000, debug=True)