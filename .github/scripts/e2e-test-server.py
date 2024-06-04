import requests
import json
import subprocess
import os
import logging
import sys
import random
import platform

n = len(sys.argv)
print("Total arguments passed:", n)
if n < 3:
    print("The number of arguments should >= 3")
    exit(1)

BINARY_PATH = sys.argv[1]
if platform.system == 'Windows':
    BINARY_PATH += '.exe'
MODEL_PATH = sys.argv[2]

CONST_CTX_SIZE = 1024
CONST_USER_ROLE = "user"
CONST_ASSISTANT_ROLE = "assistant"


        
logging.basicConfig(filename='./test.log',
                    filemode='w',
                    format='%(asctime)s,%(msecs)d %(name)s %(levelname)s %(message)s',
                    datefmt='%H:%M:%S',
                    level=logging.INFO)

chat_data = []

def RequestPost(req_data, url, is_stream = False):
    try:
        r = requests.post(url, json=req_data, stream=is_stream)
        r.raise_for_status()
        if is_stream: 
            if r.encoding is None:
                r.encoding = 'utf-8'

            res = ""
            for line in r.iter_lines(decode_unicode=True):
                if line and "[DONE]" not in line:
                    data = json.loads(line[5:])
                    content = data['choices'][0]['delta']['content']
                    res += content
            logging.info('{\'assistant\': \''  + res + '\'}')
            chat_data.append({
                "role": CONST_ASSISTANT_ROLE,
                "content": res
            })
            # Can be an error when model generates gabarge data  
            res_len = len(res.split())          
            if res_len >= CONST_CTX_SIZE - 50:
                logging.warning("Maybe generated gabarge data: " + str(res_len))
                # return False
        else:
            res_json = r.json()
            logging.info(res_json)
        
        if r.status_code == 200:
            return True
        else:
            logging.warning('{\'status_code\': '  + str(r.status_code) + '}') 
            return False
    except requests.exceptions.HTTPError as error:
        logging.error(error)
        return False

def RequestGet(url):
    try:
        r = requests.get(url)
        r.raise_for_status()       
        res_json = r.json()        
        logging.info(res_json)        
        if r.status_code == 200:
            return True
        else:
            logging.warning('{\'status_code\': '  + str(r.status_code) + '}') 
            return False
    except requests.exceptions.HTTPError as error:
        logging.error(error)
        return False

def StopServer():
    url = "http://127.0.0.1:"+ str(port) + "/destroy"
    try:
        r = requests.delete(url)
        logging.info(r.status_code)
    except requests.ConnectionError as error:
        logging.error(error)
        
def CleanUp():
    StopServer()
    p.communicate()
    with open('./test.log', 'r') as f:
        print(f.read())
    

def TestLoadChatModel():
    new_data = {
        "model_path": cwd + "/" + MODEL_PATH,
        "user_prompt": "<|user|>",
        "ai_prompt": "<|end|><|assistant|>",
    }

    url_post = "http://127.0.0.1:"+ str(port) + "/loadmodel"

    res = RequestPost(new_data, url_post)
    if not res:
        CleanUp()
        exit(1)

def TestChatCompletion():
    content = "How are you today?"
    user_msg = {
        "role": CONST_USER_ROLE,
        "content": content
    }
    logging.info('{\'user\': \''  + content + '\'}')
    
    chat_data.append(user_msg)
    new_data = {
        "frequency_penalty": 0,
        "max_tokens": CONST_CTX_SIZE,
        "messages": chat_data,
        "presence_penalty": 0,
        "stop": ["[/INST]", "</s>"],
        "stream": True,
        "temperature": 0.7,
        "top_p": 0.95
    }
    
    url_post = "http://127.0.0.1:"+ str(port) + "/v1/chat/completions"

    res = RequestPost(new_data, url_post, True)
    if not res:
        CleanUp()
        exit(1)
    
    content = "Tell me a short story"
    user_msg = {
        "role": CONST_USER_ROLE,
        "content": content
    }
    logging.info('{\'user\': \''  + content + '\'}')
    
    chat_data.append(user_msg)

    new_data = {
        "frequency_penalty": 0,
        "max_tokens": CONST_CTX_SIZE,
        "messages": chat_data,
        "presence_penalty": 0,
        "stop": ["[/INST]", "</s>"],
        "stream": True,
        "temperature": 0.7,
        "top_p": 0.95
    }

    res = RequestPost(new_data, url_post, True)
    if not res:
        CleanUp()
        exit(1)

def TestUnloadModel():
    new_data = {}

    url_post = "http://127.0.0.1:"+ str(port) + "/unloadmodel"

    res = RequestPost(new_data, url_post)
    if not res:
        CleanUp()
        exit(1)

port = random.randint(10000, 11000)

cwd = os.getcwd()
print(cwd)
p = subprocess.Popen([cwd + '/' + BINARY_PATH, '127.0.0.1', str(port)])
print("Server started!")

TestLoadChatModel()
TestChatCompletion()
TestUnloadModel()
CleanUp()

