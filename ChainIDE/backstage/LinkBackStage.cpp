#include "LinkBackStage.h"

#include <QProcess>
#include <QTimer>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTime>

#include "DataDefine.h"
#include "IDEUtil.h"
#include "popwidget/commondialog.h"

#include "datarequire/DataRequireManager.h"

static const int NODE_RPC_PORT = 60320;//node端口  test    formal = test+10
static const int CLIENT_RPC_PORT = 60321;//client端口  test    formal = test+10

class LinkBackStage::DataPrivate
{
public:
    DataPrivate(int type)
        :chaintype(type)
        ,nodeProc(new QProcess())
        ,startNode(false)
        ,clientProc(new QProcess())
        ,startClient(false)
    {
        nodePort = NODE_RPC_PORT + 10*(type-1);
        clientPort = CLIENT_RPC_PORT + 10*(type-1);

        dataPath = 1 == type ? "/testhx" : "/formalhx";

        dataRequire = new DataRequireManager("127.0.0.1",QString::number(clientPort));
    }
    ~DataPrivate()
    {
        if(dataRequire)
        {
            delete dataRequire;
            dataRequire = nullptr;
        }
        if(clientProc )
        {
            if(clientProc->state() == QProcess::Running)
            {
                clientProc->close();
            }
            delete clientProc;
            clientProc = nullptr;

        }
        if(nodeProc)
        {
            if(nodeProc->state() == QProcess::Running)
            {
                nodeProc->close();
            }
            delete nodeProc;
            nodeProc = nullptr;
        }
    }
public:
    int chaintype;
    int nodePort;
    int clientPort;
    QString dataPath;
    QProcess* nodeProc;
    bool startNode;
    QProcess* clientProc;
    bool startClient;
    DataRequireManager *dataRequire;
};

LinkBackStage::LinkBackStage(int type,QObject *parent)
        : BackStageBase(parent)
        ,_p(new DataPrivate(type))
{

}

LinkBackStage::~LinkBackStage()
{
    qDebug()<<"delete "<<_p->chaintype<<" hxstage";
    delete _p;
    _p = nullptr;
}

void LinkBackStage::startExe(const QString &appDataPath)
{
    //设置数据存储路径
    QString str = appDataPath;
    str.replace("\\","/");
    _p->dataPath =str + _p->dataPath;
    //先启动node，然后启动client
    startNodeProc();
}

bool LinkBackStage::exeRunning()
{
    return _p->clientProc->state() == QProcess::Running && _p->nodeProc->state() == QProcess::Running && _p->dataRequire->isConnected();
}

QProcess *LinkBackStage::getProcess() const
{
    return _p->clientProc;
}

void LinkBackStage::ReadyClose()
{
    if(exeRunning())
    {
        disconnect(_p->clientProc,&QProcess::stateChanged,this,&LinkBackStage::onClientExeStateChanged);
        disconnect(_p->nodeProc,&QProcess::stateChanged,this,&LinkBackStage::onNodeExeStateChanged);
        //先lock

        QSharedPointer<QEventLoop> loop = QSharedPointer<QEventLoop>(new QEventLoop());

        connect(_p->dataRequire,&DataRequireManager::requireResponse,[&loop,this](const QString &_rpcId,const QString &message){
            if(_rpcId == "id-lock-onCloseIDE")
            {
                rpcPostedSlot( "id-witness_node_stop",IDEUtil::toJsonFormat( "witness_node_stop", QJsonArray()));
            }
            else if(_rpcId == "id-witness_node_stop")
            {
                if(loop && loop->isRunning())
                {
                    //先关node，然后关client
                    _p->nodeProc->waitForFinished(-1);
                    _p->clientProc->waitForFinished(-1);
                    qDebug()<<"close hxstage "<<_p->chaintype<<" finish";
                    loop->quit();
                }
            }
        });
        rpcPostedSlot("id-lock-onCloseIDE",IDEUtil::toJsonFormat( "lock", QJsonArray()));

        loop->exec();
    }
    emit exeClosed();
}

void LinkBackStage::startNodeProc()
{
    connect(_p->nodeProc,&QProcess::stateChanged,this,&LinkBackStage::onNodeExeStateChanged);
    connect(_p->nodeProc,&QProcess::readyReadStandardError,this,&LinkBackStage::readNodeStandError);
    connect(_p->nodeProc,&QProcess::readyReadStandardOutput,this,&LinkBackStage::readNodeStandOutput);
    QStringList strList;
    strList << "--data-dir=" +_p->dataPath
            << QString("--rpc-endpoint=127.0.0.1:%1").arg(_p->nodePort)
            <<"--rewind-on-close";
    qDebug() << "start hx_node " << strList;

    if(1 == _p->chaintype)
    {//测试链
        //启动前，先判断是否需要复制config到对应目录
        QFile conf(_p->dataPath+"/config.ini");
        if(conf.exists() && conf.open(QIODevice::ReadOnly|QIODevice::Text)){
            if(!QString(conf.readAll()).contains("1.6.25")){
                conf.close();
                conf.remove();
                QFile::copy(QCoreApplication::applicationDirPath()+"/"+DataDefine::LINK_TEST_CONFIG_PATH,_p->dataPath+"/config.ini");
            }
            else{
                conf.close();
            }
        }
        else{
            QDir dir(_p->dataPath);
            if(!dir.exists()){
                dir.mkpath(_p->dataPath);
            }

            QFile::copy(QCoreApplication::applicationDirPath()+"/"+DataDefine::LINK_TEST_CONFIG_PATH,_p->dataPath+"/config.ini");
        }
        _p->nodeProc->start(QCoreApplication::applicationDirPath()+QDir::separator()+DataDefine::LINK_NODE_TEST_EXE,strList);
    }
    else if(2 == _p->chaintype)
    {//正式链
        _p->nodeProc->start(QCoreApplication::applicationDirPath()+QDir::separator()+DataDefine::LINK_NODE_EXE,strList);
    }

}

void LinkBackStage::startClientProc()
{
    connect(_p->clientProc,&QProcess::stateChanged,this,&LinkBackStage::onClientExeStateChanged);
    connect(_p->clientProc,&QProcess::readyReadStandardError,this,&LinkBackStage::readClientStandError);
    connect(_p->clientProc,&QProcess::readyReadStandardOutput,this,&LinkBackStage::readClientStandOutput);

    QStringList strList;
    strList << "--wallet-file=" + _p->dataPath + "/wallet.json"
            << QString("--server-rpc-endpoint=ws://127.0.0.1:%1").arg(_p->nodePort)
            << QString("--rpc-endpoint=127.0.0.1:%1").arg(_p->clientPort);
    qDebug()<<"start hx_client"<<strList;
    if(1 == _p->chaintype)
    {
        _p->clientProc->start(QCoreApplication::applicationDirPath()+QDir::separator()+DataDefine::LINK_CLIENT_TEST_EXE,strList);
    }
    else if(2 == _p->chaintype)
    {
        _p->clientProc->start(QCoreApplication::applicationDirPath()+QDir::separator()+DataDefine::LINK_CLIENT_EXE,strList);
    }
}

void LinkBackStage::readNodeStandError()
{
    QString str = _p->nodeProc->readAllStandardError();
    if(str.isEmpty()) return;
    qDebug() << "node exe standardError: " << str ;
    emit AdditionalOutputMessage(str);
    checkNodeMessage(str);
}

void LinkBackStage::readNodeStandOutput()
{
    QString str = _p->nodeProc->readAllStandardOutput();
    if(str.isEmpty()) return;
//    qDebug() << "node exe standardOut: " << str ;
    emit AdditionalOutputMessage(str);
//    checkNodeMessage(str);
}

void LinkBackStage::readClientStandError()
{
    QString str = _p->clientProc->readAllStandardError();
    if(str.isEmpty()) return;
    qDebug() << "client exe standardError: " << str ;
    emit AdditionalOutputMessage(str);
    checkClientMessage(str);

}

void LinkBackStage::readClientStandOutput()
{
    QString str = _p->clientProc->readAllStandardOutput();
    if(str.isEmpty()) return;
    qDebug() << "client exe standardOut: " << str ;
    emit AdditionalOutputMessage(str);
//    checkClientMessage(str);
}

void LinkBackStage::checkNodeMessage(const QString &message)
{
    if(_p->startNode) return;
    if(message.contains("Chain ID is"))
    {
        _p->startNode = true;
        startClientProc();
    }
}

void LinkBackStage::checkClientMessage(const QString &message)
{
    if(_p->startClient) return;
    if(message.contains("Listening for"))
    {
        qDebug()<<"start datarequire hx";
        _p->startClient = true;
        initSocketManager();
    }
}

void LinkBackStage::onNodeExeStateChanged()
{
    if(_p->nodeProc->state() == QProcess::Starting)
    {
    }
    else if(_p->nodeProc->state() == QProcess::Running)
    {
        qDebug() << QString("hx_node %1 is running").arg(_p->chaintype);
    }
    else if(_p->nodeProc->state() == QProcess::NotRunning)
    {
        qDebug()<<QString("hx_node %1 is notrunning :%2").arg(_p->chaintype).arg(_p->nodeProc->errorString());
        CommonDialog commonDialog(CommonDialog::OkOnly);
        commonDialog.setText(tr("Fail to launch hx_node !"));
        commonDialog.pop();
        emit exeNotRunning();
    }
}

void LinkBackStage::onClientExeStateChanged()
{
    if(_p->clientProc->state() == QProcess::Starting)
    {
    }
    else if(_p->clientProc->state() == QProcess::Running)
    {
        qDebug() << QString("hx_client %1 is running").arg(_p->chaintype);

    }
    else if(_p->clientProc->state() == QProcess::NotRunning)
    {
        qDebug() << QString("hx_client %1 not running :%2").arg(_p->chaintype).arg(_p->clientProc->errorString());
        CommonDialog commonDialog(CommonDialog::OkOnly);
        commonDialog.setText(tr("Fail to launch %1 !").arg("hx_client"));
        commonDialog.pop();
        emit exeNotRunning();
    }
}

void LinkBackStage::initSocketManager()
{
    connect(_p->dataRequire,&DataRequireManager::requireResponse,this,&LinkBackStage::rpcReceivedSlot);
    connect(_p->dataRequire,&DataRequireManager::connectFinish,this,&BackStageBase::exeStarted);

    _p->dataRequire->startManager(DataRequireManager::WEBSOCKET);
}

void LinkBackStage::rpcPostedSlot(const QString & id, const QString & cmd)
{
    _p->dataRequire->requirePosted(id,cmd);
}

void LinkBackStage::rpcReceivedSlot(const QString &id, const QString &message)
{
    QString result = message.mid( QString("{\"id\":32800,\"jsonrpc\":\"2.0\",").size());
    result = result.left( result.size() - 1);
    result.prepend("{");
    result.append("}");
    emit rpcReceived(id,result);
}
