Maria db를 선택해서 사용 중입니다.

DB 서버는 60182228-DB 인스턴스 안에서 docker의 mariadb이미지를 활용해서 아래와 같은 명령어로 container를 띄워 사용 중입니다.
sudo docker run --detach --name mariadb -p 3306:3306 -v $HOME/mariadb_volume:/var/lib/mysql --env MARIADB_ROOT_PASSWORD=ghkfud mariadb:latest
-v 옵션을 통해서 db서버가 재시작 되더라도 저장되어 있던 데이터가 유저 디렉토리에 영속적으로 유지되도록 하였습니다.

메모서버인스턴스와 db서버 간에 통신은 서브넷을 이용하여 private ip를 통해 연결하였습니다.

사용 중인 db 이름은 memo_db로 생성하였고 데베 생성과 테이블 생성문은 아래와 같습니다.
sudo docker exec -it mariadb bash
mariadb -u root -p
password 입력
create database memo_db;
use memo_db;

-- Create the APP_USER table
CREATE TABLE APP_USER (
    USER_ID VARCHAR(255) PRIMARY KEY,
    USER_NAME VARCHAR(255) NOT NULL
);

-- Create the MEMO table with AUTO_INCREMENT for MEMO_ID
CREATE TABLE MEMO (
    MEMO_ID INT AUTO_INCREMENT,
    USER_ID VARCHAR(255),
    CONTENT VARCHAR(255),
    PRIMARY KEY (MEMO_ID, USER_ID),
    FOREIGN KEY (USER_ID) REFERENCES APP_USER(USER_ID)
);

----------------------------------------------------
memp.py

띄워놓은 mariadb 접속은 mysql.connector 모듈을 이용하였습니다.
모듈 설치 : pip3 install mysql-connector-python

이후에 연결은 아래 코드를 통해 맺어주었습니다.
connection = database.connect(
    user='root',
    password='ghkfud',
    host='172.31.1.135',
    database='memo_db')

이후 아래와 같이 네이버 oauth api와 연결하기 위한 값들을 변수로 매핑했습니다.

naver_client_id = 'Z0Taie9OYDDditUhF4GC' # Client ID
naver_client_secret = 'WRGUdnVAQE' # Client Secret
naver_user_url = 'https://openapi.naver.com/v1/nid/me' # 회원 프로필 요청 url
naver_token_url = 'https://nid.naver.com/oauth2.0/token' # 토큰 요청 url
naver_redirect_uri = 'http://60182228-lb-166353545.ap-northeast-2.elb.amazonaws.com/memo/naver-oauth' # redirect url

이후 코드 구현 코드는 각 주석 섹션에 알맞게 삽입하였습니다.