#include "kcf_project/project.hpp"
#include "custom_module_dialog.hpp"
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
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollArea>
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
    std::map<std::string, QPlainTextEdit*> buildFields;
    QTableWidget* packages;
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
    kpm::Json readBuildSettings() const {
        kpm::Json settings = kpm::Json::object();
        for (const auto& [key, editor] : buildFields) {
            settings[key] = kpm::Json::array();
            for (const auto& line : editor->toPlainText().split('\n'))
                if (!line.isEmpty()) settings[key].push_back(line.toStdString());
        }
        settings["packages"] = kpm::Json::array();
        for (int row = 0; row < packages->rowCount(); ++row) {
            auto cell = [&](int col) { auto* item = packages->item(row, col); return item ? item->text().trimmed() : QString(); };
            auto* mode = qobject_cast<QComboBox*>(packages->cellWidget(row, 2));
            kpm::Json components = kpm::Json::array();
            for (const auto& component : cell(3).split(',', Qt::SkipEmptyParts))
                components.push_back(component.trimmed().toStdString());
            settings["packages"].push_back({{"name", cell(0).toStdString()}, {"version", cell(1).toStdString()},
                {"mode", mode->currentText().toStdString()}, {"components", components}});
        }
        return settings;
    }
    void addPackageRow(const kpm::Json& package) {
        int row = packages->rowCount(); packages->insertRow(row);
        packages->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(package.value("name", ""))));
        packages->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(package.value("version", ""))));
        auto* mode = new QComboBox; mode->addItems({"auto", "config", "module"});
        mode->setCurrentText(QString::fromStdString(package.value("mode", "auto"))); packages->setCellWidget(row, 2, mode);
        QStringList components;
        for (const auto& c : package.value("components", kpm::Json::array())) components << QString::fromStdString(c.get<std::string>());
        packages->setItem(row, 3, new QTableWidgetItem(components.join(", ")));
    }
    void showBuildSettings(const kpm::Json& element) {
        auto settings = kpm::element_build_settings(element);
        for (const auto& [key, editor] : buildFields) {
            QStringList lines;
            for (const auto& value : settings.at(key)) lines << QString::fromStdString(value.get<std::string>());
            editor->setPlainText(lines.join('\n'));
        }
        packages->setRowCount(0);
        for (const auto& package : settings.at("packages")) addPackageRow(package);
    }
    void readSettings() {
        if(root.empty()||elements->currentText().isEmpty())return;
        guarded([&]{auto model=kpm::load(root);for(const auto& e:model.at("elements"))if(e.at("name")==elements->currentText().toStdString()){
            showBuildSettings(e);
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
        auto* create=button("New Project",top);auto* open=button("Open Project",top);auto* regenerate=button("Preview / Update Generated Files",top);location=new QLineEdit;location->setReadOnly(true);top->addWidget(location);
        auto* split=new QSplitter;layout->addWidget(split,2);structure=new QTreeWidget;structure->setHeaderLabels({"Structure","Declaration / path"});structure->setColumnWidth(0,210);split->addWidget(structure);
        auto* settings=new QWidget;split->addWidget(settings);auto* form=new QFormLayout(settings);
        elements=new QComboBox;form->addRow("Element",elements);controls.push_back(elements);
        elementName=new QLineEdit;auto* addRow=new QHBoxLayout;addRow->addWidget(elementName);addButton=button("Add Element",addRow);form->addRow("New name",addRow);controls.push_back(elementName);
        auto* tabs = new QTabWidget; form->addRow(tabs);
        auto* runtime = new QWidget; tabs->addTab(runtime, "Runtime");
        auto* runtimeForm = new QFormLayout(runtime);
        frequency=new QDoubleSpinBox;frequency->setRange(0.001,100000);frequency->setDecimals(3);runtimeForm->addRow("Loop Hz",frequency);
        supervised=new QCheckBox("Include in Supervisor");runtimeForm->addRow(supervised);
        auto timeout=[&](const QString& label){auto* s=new QSpinBox;s->setRange(1,3600000);runtimeForm->addRow(label,s);return s;};
        startup=timeout("Startup timeout (ms)");health=timeout("Health timeout (ms)");shutdown=timeout("Shutdown timeout (ms)");
        workdir=new QLineEdit(".");runtimeForm->addRow("Working directory",workdir);
        arguments=new QPlainTextEdit("[]");arguments->setMaximumHeight(60);runtimeForm->addRow("Arguments (JSON array)",arguments);
        environment=new QPlainTextEdit("{}");environment->setMaximumHeight(60);runtimeForm->addRow("Environment (JSON object)",environment);
        auto* buildPage = new QWidget;
        auto* buildScroll = new QScrollArea; buildScroll->setWidgetResizable(true); buildScroll->setWidget(buildPage);
        tabs->addTab(buildScroll, "CMake Build");
        auto* buildForm = new QFormLayout(buildPage);
        auto* hint = new QLabel("One literal value per line. Paths are relative to the project root.\nAll target settings use PRIVATE scope; advanced CMake stays in CMakeLists.txt.");
        hint->setWordWrap(true); buildForm->addRow(hint);
        struct BuildField { const char* key; const char* label; const char* example; const char* help; };
        const BuildField fields[] = {
            {"sources", "Additional C++ sources", "ex) test_module/module1/src/module1.cpp",
             "Actual files explicitly compiled by target_sources. One file per line; a directory alone does not add its files."},
            {"include_directories", "Include directories", "ex) test_module/module1/include",
             "Header search directories for #include. This does not compile any source files."},
            {"link_libraries", "Link libraries / CMake targets", "ex) Eigen3::Eigen",
             "Library names, exported CMake targets, or library file paths containing /. Register packages in the table when required."},
            {"compile_definitions", "Compile definitions (NAME=value)", "ex) USE_IMU=1",
             "Preprocessor definitions: NAME or NAME=value, without the -D prefix."},
            {"compile_options", "Compile options", "ex) -Wall",
             "Compiler options for this Element. One literal option per line."},
            {"link_options", "Link options", "ex) -pthread",
             "Linker-driver options for this Element. One literal option per line; compiler-specific availability applies."}};
        const QString pathHelp = "\nPaths are relative to the project root (not module/source directory); explicitly supplied absolute paths are allowed. Multiple values use separate lines.";
        for (const auto& field : fields) {
            auto* editor = new QPlainTextEdit; editor->setMaximumHeight(55);
            editor->setPlaceholderText(field.example); editor->setToolTip(QString(field.help) + pathHelp);
            buildFields[field.key] = editor; buildForm->addRow(field.label, editor);
        }
        auto* customModule = new QPushButton("Add Custom Module"); buildForm->addRow(customModule);
        connect(customModule, &QPushButton::clicked, this, [this] {
            guarded([&] {
                if (elements->currentText().isEmpty()) throw std::runtime_error("Select an Element first");
                CustomModuleDialog dialog(root, readBuildSettings(), elements->currentText().toStdString(), this);
                if (dialog.exec() == QDialog::Accepted)
                    showBuildSettings(kpm::Json{{"build", dialog.settings()}});
            });
        });
        packages = new QTableWidget(0, 4); packages->setHorizontalHeaderLabels({"Package", "Min version", "Mode", "Components (comma separated)"});
        packages->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        packages->setMinimumHeight(120); buildForm->addRow("Required find_package", packages);
        auto* packageButtons = new QHBoxLayout;
        auto* addPackage = new QPushButton("Add package"); auto* removePackage = new QPushButton("Remove selected package");
        packageButtons->addWidget(addPackage); packageButtons->addWidget(removePackage); buildForm->addRow(packageButtons);
        connect(addPackage, &QPushButton::clicked, this, [this]{ addPackageRow(kpm::Json::object()); });
        connect(removePackage, &QPushButton::clicked, this, [this]{ if (packages->currentRow() >= 0) packages->removeRow(packages->currentRow()); });
        auto* saveRow=new QHBoxLayout;saveButton=button("Preview / Save Settings",saveRow);form->addRow(saveRow);
        controls.push_back(settings);
        auto* builds=new QHBoxLayout;layout->addLayout(builds);buildDir=new QLineEdit("build");config=new QComboBox;config->addItems({"Debug","Release","RelWithDebInfo","MinSizeRel"});prefix=new QLineEdit;prefix->setPlaceholderText("Optional installed SDK prefix (CMAKE_PREFIX_PATH)");
        builds->addWidget(new QLabel("Build directory"));builds->addWidget(buildDir);builds->addWidget(config);builds->addWidget(prefix);controls.insert(controls.end(),{buildDir,config,prefix});
        auto* actions=new QHBoxLayout;layout->addLayout(actions);buildAll=button("Build All",actions);buildTarget=button("Build Element",actions);runElement=button("Run Element",actions);runSupervisor=button("Run Supervisor",actions);stop=new QPushButton("Stop owned process");actions->addWidget(stop);stop->setEnabled(false);
        logs=new QPlainTextEdit;logs->setReadOnly(true);logs->setMaximumBlockCount(10000);layout->addWidget(logs,1);
        connect(create,&QPushButton::clicked,this,[&]{auto parent=QFileDialog::getExistingDirectory(this,"Parent directory");if(parent.isEmpty())return;bool ok=false;auto name=QInputDialog::getText(this,"New Project","Name (lowercase identifier)",QLineEdit::Normal,{},&ok);if(ok)guarded([&]{createAt(kpm::fs::path(parent.toStdString())/name.toStdString(),name.toStdString());});});
        connect(open,&QPushButton::clicked,this,[&]{auto path=QFileDialog::getExistingDirectory(this,"Open project directory");if(!path.isEmpty())guarded([&]{auto candidate=kpm::fs::path(path.toStdString());kpm::load(candidate);root=candidate;refresh();});});
        connect(regenerate,&QPushButton::clicked,this,[&]{guarded([&]{if(preview(kpm::regenerate(root,true))){kpm::regenerate(root);refresh();}});});
        connect(elements,&QComboBox::currentTextChanged,this,[&]{readSettings();});
        connect(addButton,&QPushButton::clicked,this,[&]{guarded([&]{auto n=elementName->text().toStdString();if(preview(kpm::add(root,n,true))){kpm::add(root,n);refresh();elements->setCurrentText(QString::fromStdString(n));}});});
        connect(saveButton,&QPushButton::clicked,this,[&]{guarded([&]{kpm::Json patch{{"hz",frequency->value()},{"argv",kpm::Json::parse(arguments->toPlainText().toStdString())},{"env",kpm::Json::parse(environment->toPlainText().toStdString())},{"workdir",workdir->text().toStdString()},{"supervised",supervised->isChecked()},{"startup_timeout_ms",startup->value()},{"health_timeout_ms",health->value()},{"shutdown_timeout_ms",shutdown->value()},{"build",readBuildSettings()}};auto n=elements->currentText().toStdString();if(preview(kpm::configure_element(root,n,patch,true))){kpm::configure_element(root,n,patch);refresh();}});});
        connect(buildAll,&QPushButton::clicked,this,[&]{operation(true,true);});connect(buildTarget,&QPushButton::clicked,this,[&]{operation(true,false);});connect(runElement,&QPushButton::clicked,this,[&]{operation(false,false);});connect(runSupervisor,&QPushButton::clicked,this,[&]{operation(false,true);});connect(stop,&QPushButton::clicked,this,[&]{cancel=true;});
    }
    ~Window() override {cancel=true;if(worker.joinable())worker.join();}
    void selfTest(const std::string& path,const std::string& sdk) {
        automation=true;prefix->setText(QString::fromStdString(sdk));
        guarded([&]{createAt(path,"gui_fixture");elementName->setText("driver");addButton->click();frequency->setValue(25);arguments->setPlainText("[\"gui argument\"]");
            kpm::fs::create_directories(root/"extras");
            std::ofstream(root/"extras/gui.hpp") << "#pragma once\nint gui_extra();\n";
            std::ofstream(root/"extras/gui.cpp") << "#include <gui.hpp>\n#ifndef GUI_BUILD_SETTING\n#error Missing GUI definition\n#endif\nint gui_extra() { return GUI_BUILD_SETTING; }\n";
            std::ofstream(root/"extras/unselected.cpp") << "#error Unselected source must never be compiled\n";
            CustomModuleDialog module(root, readBuildSettings(), "driver", this);
            kpm::Json selection{{"module_directory","extras"},{"source_directory","extras"},
                {"include_directories",kpm::Json::array({"extras"})},{"sources",kpm::Json::array({"extras/missing.cpp"})}};
            module.setSelection(selection);
            if (module.validateSelection()) throw std::runtime_error("Missing source accepted");
            selection["sources"] = kpm::Json::array({"extras/gui.cpp", "extras/./gui.cpp"});
            module.setSelection(selection);
            if (module.validateSelection()) throw std::runtime_error("Duplicate source accepted");
            selection["sources"] = kpm::Json::array({"extras/gui.cpp"}); module.setSelection(selection);
            if (!module.validateSelection()) throw std::runtime_error("Valid module rejected");
            QTimer::singleShot(0, &module, [&module] {
                for (auto* button : module.findChildren<QPushButton*>())
                    if (button->text() == "Add to build settings") button->click();
            });
            if (module.exec() != QDialog::Accepted) throw std::runtime_error("Module dialog did not accept");
            showBuildSettings(kpm::Json{{"build",module.settings()}});
            buildFields.at("link_libraries")->setPlainText("Threads::Threads");
            buildFields.at("compile_definitions")->setPlainText("GUI_BUILD_SETTING=7");
            buildFields.at("compile_options")->setPlainText("-Wall");
            buildFields.at("link_options")->setPlainText("-Wl,--as-needed");
            addPackageRow({{"name","Threads"},{"mode","module"}});
            saveButton->click();});
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
