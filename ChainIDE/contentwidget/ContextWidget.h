#ifndef CONTEXTWIDGET_H
#define CONTEXTWIDGET_H

#include <vector>
#include <QWidget>
#include <QPoint>

namespace Ui {
class ContextWidget;
}

class Editor;
class ContextWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ContextWidget(QWidget *parent = nullptr);
    ~ContextWidget();
public:
    void undo();

    void redo();

    bool isUndoAvailable();

    bool isRedoAvailable();

    bool hasFileUnsaved();

    bool currentFileUnsaved();

    const QString &getCurrentFilePath()const;

    void TabBreakPoint();//调整断点，有则删，无则加
    void AddBreakPoint(const QString &path,int linenumber);//强行添加断点
    void RemoveBreakPoint(const QString &path,int linenumber);//强行删除断点
    void ClearBreakPoint();//清空断点

    void SetDebuggerLine(const QString &path,int linenumber);//设置调试期断点样式
    void ClearDebuggerLine(const QString &path);//清空所有断点样式

    void JumpToLine(const QString &path,int linenumber,int ch);//文件名，行号，字符定位数
signals:
    void textChanged();

    void fileSelected(QString path);

    void contentStateChange();

    void GetBreakPointFinish(const QString &filePath,const std::vector<int> &data);
public slots:
    void showFile(QString path);

    bool closeFile(QString path);       // 取消了就返回false

    bool closeAll();                    // 取消了就返回false

    void saveFile();

    void saveFile(QString path);

    void saveAll();

    void onTextChanged();

    void gutterRightClickedSignal();

    void CheckDeleteFile();//文件被删除后使用，用于检查是否有被删除的文件正在被打开，是则关闭

    void markChangedSlots(int lineNumber,bool isAdd);//断点添加或者删除的槽处理

    void GetBreakPointSlots(const QString &filePath);//获取某个文件的断点行号
private slots:
    void currentTabChanged(int i);
    void tabCloseRquest(int i);
    void createTabMenu(const QPoint &pos);
private:
    bool saveFile(int i);
    bool closeFile(int i);

    int getTabNumber(const QString &path)const;
    const QString &getPathFromNumber(int i)const;
    Editor *getCurrentEditor()const;
    Editor *getEditor(int i)const;
    Editor *getEditor(const QString &path)const;

    void contextUpdate();
private:
    void InitWidget();
private:
    Ui::ContextWidget *ui;
private:
    class DataPrivate;
    DataPrivate *_p;
};

#endif // CONTEXTWIDGET_H
