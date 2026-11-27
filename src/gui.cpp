#include "kcf_project/project.hpp"
#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QCloseEvent>
#include <QTimer>
#include <QLabel>
#include <thread>
#include <fstream>

class Window : public QMainWindow {
    kpm::fs::path root;
    QLineEdit *location,*elementName,*prefix,*buildDir,*workdir;
    QComboBox *elements,*config;
    QDoubleSpinBox* frequency;
    QSpinBox *startup,*health,*shutdown;
    QCheckBox* supervised;
    QPlainTextEdit *arguments,*environment,*logs;
    QTreeWidget* structure;
    std::vector<QWidget*> controls;
    std::thread worker;
    std::atomic_bool cancel{false};
    bool busy=false, automation=false;
    int lastResult=0,phase=0,ticks=0;
    QPushButton *addButton,*saveButton,*buildAll,*buildTarget,*runElement,*runSupervisor,*stop;
    void error(const std::exception& e) {lastResult=2;logs->appendPlainText(e.what());if(!automation)QMessageBox::critical(this,"Project Manager",e.what());}
    template<class F> void guarded(F f) {try{f();}catch(const std::exception& e){error(e);}}
    void refresh() {
        auto m=kpm::load(root);location->setText(QString::fromStdString(root.string()));
        std::string previous=elements->currentText().toStdString();elements->clear();structure->clear();
        auto* project=new QTreeWidgetItem(structure,{QString::fromStdString(m.at("name")),"Design-time declaration"});
        auto* supervisor=new QTreeWidgetItem(project,{"Supervisor",QString::fromStdString(m.at("name").get<std::string>()+"_supervisor")});
        for(const auto& e:m.at("elements")) {
            auto n=e.at("name").get<std::string>();elements->addItem(QString::fromStdString(n));
            auto* node=new QTreeWidgetItem(e.at("supervised").get<bool>()?supervisor:project,{QString::fromStdString(n),QString::fromStdString(e.at("id"))});
            new QTreeWidgetItem(node,{"Class",QString::fromStdString("Element_"+n)});
            new QTreeWidgetItem(node,{"Source",QString::fromStdString("src/"+n+"/"+n+".cpp")});
            new QTreeWidgetItem(node,{"Header",QString::fromStdString("include/"+m.at("name").get<std::string>()+"/"+n+".hpp")});
            new QTreeWidgetItem(node,{"CMake target",QString::fromStdString(e.at("target"))});
            QString binary="Configure/build to resolve";
            try{binary=QString::fromStdString(kpm::executable(root,buildDir->text().toStdString(),config->currentText().toStdString(),e.at("target")).string());}catch(const std::exception&){}
            new QTreeWidgetItem(node,{"Executable",binary});
        }
        int index=elements->findText(QString::fromStdString(previous));if(index>=0)elements->setCurrentIndex(index);
        structure->expandAll();readSettings();
    }
    void readSettings() {
        if(root.empty()||elements->currentText().isEmpty())return;
        guarded([&]{auto model=kpm::load(root);for(const auto& e:model.at("elements"))if(e.at("name")==elements->currentText().toStdString()){
            frequency->setValue(e.at("hz"));startup->setValue(e.at("startup_timeout_ms"));health->setValue(e.at("health_timeout_ms"));shutdown->setValue(e.at("shutdown_timeout_ms"));supervised->setChecked(e.at("supervised"));workdir->setText(QString::fromStdString(e.at("workdir")));arguments->setPlainText(QString::fromStdString(e.at("argv").dump()));environment->setPlainText(QString::fromStdString(e.at("env").dump()));
        }});
    }
    bool preview(const kpm::Changes& changes) {
        if(automation){logs->appendPlainText("Preview: "+QString::number(changes.size())+" changed files");return true;}
        QDialog dialog(this);dialog.setWindowTitle("Review generated file changes");dialog.resize(850,600);
        QVBoxLayout layout(&dialog);QPlainTextEdit text;text.setReadOnly(true);text.setPlainText(QString::fromStdString(kpm::diff(changes)));layout.addWidget(&text);
        QDialogButtonBox buttons(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);layout.addWidget(&buttons);
        connect(buttons.button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);
        connect(buttons.button(QDialogButtonBox::Cancel),&QPushButton::clicked,&dialog,&QDialog::reject);
        return dialog.exec()==QDialog::Accepted;
    }
    void createAt(const kpm::fs::path& path,const std::string& name) {
        if(preview(kpm::create(path,name,true))){kpm::create(path,name);root=kpm::fs::absolute(path);refresh();}
    }
    void operation(bool build,bool all) {
        if(busy)return;
        guarded([&]{
            auto model=kpm::load(root);std::string selected=elements->currentText().toStdString(),target;
            if(!all&&selected.empty())throw std::runtime_error("Select an Element first");
            for(const auto& e:model.at("elements"))if(e.at("name")==selected)target=e.at("target");
            auto dir=buildDir->text().toStdString(),cfg=config->currentText().toStdString(),sdk=prefix->text().toStdString();auto path=root;
            if(worker.joinable())worker.join();cancel=false;busy=true;lastResult=0;for(auto* c:controls)c->setEnabled(false);stop->setEnabled(true);
            worker=std::thread([this,path,dir,cfg,sdk,target,selected,build,all]{
                int result=2;auto log=[this](const std::string& s){QMetaObject::invokeMethod(this,[this,s]{logs->moveCursor(QTextCursor::End);logs->insertPlainText(QString::fromStdString(s));},Qt::QueuedConnection);};
                try{result=build?kpm::build(path,dir,cfg,all?"":target,sdk,log,&cancel):kpm::run(path,dir,cfg,all?"":selected,log,&cancel);}catch(const std::exception& e){log(std::string(e.what())+"\n");}
                QMetaObject::invokeMethod(this,[this,result]{lastResult=result;busy=false;for(auto* c:controls)c->setEnabled(true);stop->setEnabled(false);guarded([&]{refresh();});},Qt::QueuedConnection);
            });
        });
    }
protected:
    void closeEvent(QCloseEvent* e) override {
        if(busy){cancel=true;logs->appendPlainText("Stopping owned process; close again when it exits.");e->ignore();}else e->accept();
    }
public:
    Window() {
        setWindowTitle("KCF Project Manager — design-time configuration");resize(1200,850);
        auto* central=new QWidget;setCentralWidget(central);auto* layout=new QVBoxLayout(central);
        auto* top=new QHBoxLayout;layout->addLayout(top);
        auto button=[&](const QString& text,QHBoxLayout* row){auto* b=new QPushButton(text);row->addWidget(b);controls.push_back(b);return b;};
        auto* create=button("New Project",top);auto* open=button("Open Project",top);location=new QLineEdit;location->setReadOnly(true);top->addWidget(location);
        auto* split=new QSplitter;layout->addWidget(split,2);structure=new QTreeWidget;structure->setHeaderLabels({"Structure","Declaration / path"});structure->setColumnWidth(0,210);split->addWidget(structure);
        auto* settings=new QWidget;split->addWidget(settings);auto* form=new QFormLayout(settings);
        elements=new QComboBox;form->addRow("Element",elements);controls.push_back(elements);
        elementName=new QLineEdit;auto* addRow=new QHBoxLayout;addRow->addWidget(elementName);addButton=button("Add Element",addRow);form->addRow("New name",addRow);controls.push_back(elementName);
        frequency=new QDoubleSpinBox;frequency->setRange(0.001,100000);frequency->setDecimals(3);form->addRow("Loop Hz",frequency);
        supervised=new QCheckBox("Include in Supervisor");form->addRow(supervised);
        auto timeout=[&](const QString& label){auto* s=new QSpinBox;s->setRange(1,3600000);form->addRow(label,s);return s;};
        startup=timeout("Startup timeout (ms)");health=timeout("Health timeout (ms)");shutdown=timeout("Shutdown timeout (ms)");
        workdir=new QLineEdit(".");form->addRow("Working directory",workdir);
        arguments=new QPlainTextEdit("[]");arguments->setMaximumHeight(60);form->addRow("Arguments (JSON array)",arguments);
        environment=new QPlainTextEdit("{}");environment->setMaximumHeight(60);form->addRow("Environment (JSON object)",environment);
        auto* saveRow=new QHBoxLayout;saveButton=button("Preview / Save Settings",saveRow);form->addRow(saveRow);
        controls.push_back(settings);
        auto* builds=new QHBoxLayout;layout->addLayout(builds);buildDir=new QLineEdit("build");config=new QComboBox;config->addItems({"Debug","Release","RelWithDebInfo","MinSizeRel"});prefix=new QLineEdit;prefix->setPlaceholderText("Optional installed SDK prefix (CMAKE_PREFIX_PATH)");
        builds->addWidget(new QLabel("Build directory"));builds->addWidget(buildDir);builds->addWidget(config);builds->addWidget(prefix);controls.insert(controls.end(),{buildDir,config,prefix});
        auto* actions=new QHBoxLayout;layout->addLayout(actions);buildAll=button("Build All",actions);buildTarget=button("Build Element",actions);runElement=button("Run Element",actions);runSupervisor=button("Run Supervisor",actions);stop=new QPushButton("Stop owned process");actions->addWidget(stop);stop->setEnabled(false);
        logs=new QPlainTextEdit;logs->setReadOnly(true);logs->setMaximumBlockCount(10000);layout->addWidget(logs,1);
        connect(create,&QPushButton::clicked,this,[&]{auto parent=QFileDialog::getExistingDirectory(this,"Parent directory");if(parent.isEmpty())return;bool ok=false;auto name=QInputDialog::getText(this,"New Project","Name (lowercase identifier)",QLineEdit::Normal,{},&ok);if(ok)guarded([&]{createAt(kpm::fs::path(parent.toStdString())/name.toStdString(),name.toStdString());});});
        connect(open,&QPushButton::clicked,this,[&]{auto path=QFileDialog::getExistingDirectory(this,"Open project directory");if(!path.isEmpty())guarded([&]{auto candidate=kpm::fs::path(path.toStdString());kpm::load(candidate);root=candidate;refresh();});});
        connect(elements,&QComboBox::currentTextChanged,this,[&]{readSettings();});
        connect(addButton,&QPushButton::clicked,this,[&]{guarded([&]{auto n=elementName->text().toStdString();if(preview(kpm::add(root,n,true))){kpm::add(root,n);refresh();elements->setCurrentText(QString::fromStdString(n));}});});
        connect(saveButton,&QPushButton::clicked,this,[&]{guarded([&]{kpm::Json patch{{"hz",frequency->value()},{"argv",kpm::Json::parse(arguments->toPlainText().toStdString())},{"env",kpm::Json::parse(environment->toPlainText().toStdString())},{"workdir",workdir->text().toStdString()},{"supervised",supervised->isChecked()},{"startup_timeout_ms",startup->value()},{"health_timeout_ms",health->value()},{"shutdown_timeout_ms",shutdown->value()}};auto n=elements->currentText().toStdString();if(preview(kpm::configure_element(root,n,patch,true))){kpm::configure_element(root,n,patch);refresh();}});});
        connect(buildAll,&QPushButton::clicked,this,[&]{operation(true,true);});connect(buildTarget,&QPushButton::clicked,this,[&]{operation(true,false);});connect(runElement,&QPushButton::clicked,this,[&]{operation(false,false);});connect(runSupervisor,&QPushButton::clicked,this,[&]{operation(false,true);});connect(stop,&QPushButton::clicked,this,[&]{cancel=true;});
    }
    ~Window() override {cancel=true;if(worker.joinable())worker.join();}
    void selfTest(const std::string& path,const std::string& sdk) {
        automation=true;prefix->setText(QString::fromStdString(sdk));
        guarded([&]{createAt(path,"gui_fixture");elementName->setText("driver");addButton->click();frequency->setValue(25);arguments->setPlainText("[\"gui argument\"]");saveButton->click();});
        auto* timer=new QTimer(this);timer->setInterval(100);
        connect(timer,&QTimer::timeout,this,[this,timer]{
            ++ticks;
            if(ticks>1800){cancel=true;if(!busy){timer->stop();QApplication::exit(3);}return;}
            if(lastResult!=0&&!busy){logs->appendPlainText("GUI self-test FAIL");std::ofstream(root/"gui-test.log")<<logs->toPlainText().toStdString();timer->stop();QApplication::exit(2);return;}
            if(phase==0&&!busy){buildAll->click();phase=1;}
            else if(phase==1&&!busy){buildTarget->click();phase=2;}
            else if(phase==2&&!busy){runElement->click();phase=3;ticks=0;}
            else if(phase==3&&ticks>=15){if(!busy){lastResult=2;return;}stop->click();phase=4;}
            else if(phase==4&&!busy){runSupervisor->click();phase=5;ticks=0;}
            else if(phase==5&&ticks>=20){if(!busy){lastResult=2;return;}stop->click();phase=6;}
            else if(phase==6&&!busy){logs->appendPlainText("GUI self-test PASS: create/add/settings/build-all/build-target/run/launch/stop");std::ofstream(root/"gui-test.log")<<logs->toPlainText().toStdString();timer->stop();QApplication::exit(0);}
        });timer->start();
    }
};
int main(int argc,char** argv) {QApplication app(argc,argv);Window window;window.show();if(argc==4&&std::string(argv[1])=="--self-test")QTimer::singleShot(0,&window,[&]{window.selfTest(argv[2],argv[3]);});return app.exec();}
