/*****************************************************************************
 **                                                                         **
 **  This file is part of GlassPlotter.                                     **
 **                                                                         **
 **  GlassPlotter is free software: you can redistribute it and/or modify   **
 **  it under the terms of the GNU General Public License as published by   **
 **  the Free Software Foundation, either version 3 of the License, or      **
 **  (at your option) any later version.                                    **
 **                                                                         **
 **  GlassPlotter is distributed in the hope that it will be useful,        **
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of         **
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          **
 **  GNU General Public License for more details.                           **
 **                                                                         **
 **  You should have received a copy of the GNU General Public License      **
 **  along with GlassPlotter.  If not, see <http://www.gnu.org/licenses/>.  **
 **                                                                         **
 *****************************************************************************
 **  Author  : Hiiragi                                                      **
 **  Contact : heterophyllus.work@gmail.com                                 **
 **  Website : https://github.com/heterophyllus/glassplotter                **
 **  Date    : 2020-1-25                                                    **
 *****************************************************************************/


#include "dndt_plot_form.h"
#include "ui_dndt_plot_form.h"

#include "glass.h"
#include "glass_catalog.h"
#include "glass_selection_dialog.h"


DnDtPlotForm::DnDtPlotForm(QList<GlassCatalog*> catalogList, QWidget *parent) :
    PropertyPlotForm(parent),
    ui(new Ui::DnDtPlotForm)
{
    ui->setupUi(this);
    this->setWindowTitle("Dn/Dt(abs) Plot");

    m_catalogList = catalogList;

    m_customPlot = ui->widget;
    m_customPlot->setInteractions(QCP::iSelectAxes | QCP::iSelectLegend | QCP::iSelectPlottables);
    m_customPlot->xAxis->setLabel("Temperature(C)");
    m_customPlot->yAxis->setLabel("Dn/Dt(abs)  [10^(-6) /K]");
    m_customPlot->legend->setVisible(true);
    m_customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignRight|Qt::AlignBottom); // Legend position

    m_maxGraphCount = 7;

    m_chkLegend = ui->checkBox_Legend;

    QObject::connect(ui->pushButton_SetGlass,   SIGNAL(clicked()),     this, SLOT(setGlass()));
    QObject::connect(ui->pushButton_AddGraph,   SIGNAL(clicked()),     this, SLOT(addGraph()));
    QObject::connect(ui->pushButton_DeleteGraph,SIGNAL(clicked()),     this, SLOT(deleteGraph()));
    QObject::connect(ui->pushButton_SetAxis,    SIGNAL(clicked()),     this, SLOT(setAxis()));
    QObject::connect(ui->pushButton_Clear,      SIGNAL(clicked()),     this, SLOT(clearAll()));
    QObject::connect(ui->checkBox_Legend,       SIGNAL(toggled(bool)), this, SLOT(setLegendVisible()));

    m_editPlotStep = ui->lineEdit_PlotStep;
    m_editPlotStep->setValidator(new QDoubleValidator(0, 100, 2, this));
    m_editPlotStep->setText(QString::number(5));

    m_plotDataTable = ui->tableWidget;

    // default axis setup
    m_editXmin = ui->lineEdit_Xmin;
    m_editXmax = ui->lineEdit_Xmax;
    m_editYmin = ui->lineEdit_Ymin;
    m_editYmax = ui->lineEdit_Ymax;
    m_defaultXrange = QCPRange(-100, 140);
    m_defaultYrange = QCPRange(0.0, 23.0);

    setDefault();
}

DnDtPlotForm::~DnDtPlotForm()
{
    m_currentGlass = nullptr;
    m_catalogList.clear();
    m_wvlList.clear();
    m_plotDataTable->clear();
    m_plotDataTable = nullptr;
    m_customPlot->clearGraphs();
    m_customPlot->clearItems();
    m_customPlot->clearPlottables();
    m_customPlot = nullptr;

    delete ui;
}

void DnDtPlotForm::setGlass()
{
    GlassSelectionDialog *dlg = new GlassSelectionDialog(m_catalogList, this);
    if(dlg->exec() == QDialog::Accepted)
    {
        int catalogIndex = dlg->getCatalogIndex();
        QString glassName = dlg->getGlassName();
        Glass* newGlass = m_catalogList[catalogIndex]->glass(glassName);

        // check whether the glass contains valid thermal data
        QVector<double> vTemp = newGlass->getThermalData();
        for(int i = 0; i < vTemp.size(); i++){
            if(qIsNaN(vTemp[i])){
                QMessageBox::information(this,tr("Error"), tr("This glass does not have thermal data."));
                newGlass = nullptr;

                return;
            }
        }

        clearAll();
        m_currentGlass = newGlass;
        ui->label_GlassName->setText( m_currentGlass->name() + "_" + m_currentGlass->supplyer() );

        // plot for default wavelength
        m_wvlList  = QList<double>({435.8, 546.1, 587.0, 852.1, 1060.0});
        updateAll();

        // rescale y axis
        m_customPlot->yAxis->rescale();
        m_customPlot->replot();

        int digit = 1;
        ui->lineEdit_Xmin->setText(QString::number(m_customPlot->xAxis->range().lower, 'f', digit));
        ui->lineEdit_Xmax->setText(QString::number(m_customPlot->xAxis->range().upper, 'f', digit));
        ui->lineEdit_Ymin->setText(QString::number(m_customPlot->yAxis->range().lower, 'f', digit));
        ui->lineEdit_Ymax->setText(QString::number(m_customPlot->yAxis->range().upper, 'f', digit));

    }

    try {
        delete dlg;
    }  catch (...) {
        dlg = nullptr;
    }
    dlg = nullptr;
}

void DnDtPlotForm::addGraph()
{
    if(!m_currentGlass){
        QMessageBox::information(this,tr("Error"), tr("Set glass from the button."));
        return;
    }

    bool ok;
    double wvl = QInputDialog::getDouble(
               this,
               tr("Input new wavelength"),
               tr("wavelength(nm):"),
               587.0,
               0.0,
               10000,
               2,
               &ok
           );
    if(!ok) return;

    m_wvlList.append(wvl);
    updateAll();
}

void DnDtPlotForm::deleteGraph()
{
    if(m_customPlot->selectedGraphs().size() > 0)
    {
        QCPGraph* selectedGraph = m_customPlot->selectedGraphs().at(0);
        QString graphName = selectedGraph->name();
        int wvlCount = m_wvlList.size();
        for(int i = 0;i < wvlCount; i++){
            if(abs(m_wvlList[i] - graphName.toDouble()) < 0.1 ){
                m_wvlList.removeAt(i);
                break;
            }
        }
        updateAll();
    }
}


void DnDtPlotForm::updateAll()
{
    if(!m_currentGlass) return;

    m_customPlot->clearGraphs();
    m_customPlot->clearItems();
    m_customPlot->clearPlottables();
    m_plotDataTable->clear();

    double          plotStep = m_editPlotStep->text().toDouble();
    QVector<double> xdata    = getVectorFromRange(m_customPlot->xAxis->range(), plotStep);
    QVector<double> ydata;
    QCPGraph*       graph;

    int i,j;
    int rowCount    = xdata.size();
    int columnCount = m_wvlList.size() + 1; // temperature + wavelengths
    m_plotDataTable->setRowCount(rowCount);
    m_plotDataTable->setColumnCount(columnCount);

    int digit = ui->spinBox_Digit->value();

    QStringList header = QStringList() << "Temperature";

    // replot all graphs and recreate tables
    double currentWvl;
    for(i = 0; i < m_wvlList.size(); i++)
    {
        currentWvl = m_wvlList[i]; // unit:nm

        // graphs
        ydata = scaleVector(m_currentGlass->dn_dt_abs(xdata, currentWvl/1000.0), pow(10,6)); //unit:micron
        graph = m_customPlot->addGraph();
        graph->setName(QString::number(currentWvl));
        graph->setData(xdata, ydata);
        graph->setPen(QPen(getColorFromIndex(i,m_maxGraphCount)));
        graph->setVisible(true);

        // table
        header << QString::number(currentWvl) + "nm";
        for(j = 0; j< rowCount; j++)
        {
            addTableItem(j, 0,   QString::number(xdata[j]));         // temperature
            addTableItem(j, i+1, QString::number(ydata[j], 'f', digit)); // dn/dt(abs)
        }
    }
    m_customPlot->replot();
    m_plotDataTable->setHorizontalHeaderLabels(header);
}

void DnDtPlotForm::clearAll()
{
    m_wvlList.clear();

    m_customPlot->clearGraphs();
    m_customPlot->clearItems();
    m_customPlot->clearPlottables();
    m_customPlot->replot();

    m_plotDataTable->clear();
    m_plotDataTable->update();
}
