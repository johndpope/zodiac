﻿#include <QGraphicsView>
#include <QVBoxLayout>
#include <QEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QDebug>
#include <math.h>
#include <Astroprocessor/Output>
#include <Astroprocessor/Calc>
#include "chart.h"


RotatingCircleItem :: RotatingCircleItem(QRect rect, const QPen& pen) : QAbstractGraphicsShapeItem()
 {
  file = 0;
  this->rect = rect;
  setPen(pen);
 }

void RotatingCircleItem :: paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
 {                                            // simply draw a circle with assigned pen
  p->setPen(pen());
  int adjust = pen().width() / 2;
  p->drawEllipse(rect.adjusted(adjust, adjust, -adjust, -adjust));
 }

QPainterPath RotatingCircleItem :: shape() const
 {                                            // creates a ring shape
  QPainterPath path;
  path.addEllipse(boundingRect());

  QPainterPath innerPath;
  path.addEllipse(boundingRect().adjusted(pen().width(), pen().width(), -pen().width(), -pen().width()));

  return path.subtracted(innerPath);
 }

float RotatingCircleItem :: angle(const QPointF& pos)
 {
  QPointF center = boundingRect().center();

  float ret = atan((pos.y() - center.y()) /
                   (pos.x() - center.x())) * 180 / 3.1416;

  if (pos.x() > center.x())       // I, IV quadrant
    ret += 180;
  else if (pos.y() > center.y())  // III quadrant
    ret += 360;

  return ret;
 }

bool RotatingCircleItem :: sceneEvent(QEvent *event)
 {
  if (!file) return false;

  if (event->type() == QEvent::GraphicsSceneMousePress)
   {
    dragAngle = angle(((QGraphicsSceneMouseEvent*)event)->scenePos());
    dragDT = file->getGMT();
   }
  else if (event->type() == QEvent::GraphicsSceneMouseMove)
   {
    QGraphicsSceneMouseEvent* moveEvent = (QGraphicsSceneMouseEvent*)event;
    QPointF lastPos = moveEvent->lastScenePos();
    QPointF newPos  = moveEvent->scenePos();

    float lastAngle = angle(lastPos);
    float newAngle  = angle(newPos);

    if ((lastAngle < 10 && newAngle  > 350) ||         // new angle turns 0/360
        (newAngle  < 10 && lastAngle > 350))
     {
      dragAngle = newAngle;
      dragDT = file->getGMT();
     }

    file->setGMT(dragDT.addSecs((newAngle - dragAngle) * 180));
   }

  return true;
 }

bool RotatingCircleItem :: sceneEventFilter(QGraphicsItem *watched, QEvent *event)
 {
  if (event->type() == QEvent::GraphicsSceneHoverEnter)   // show help when mouse over item
   {
    QString tag = watched->data(0).toString();
    ((Chart*)(scene()->views()[0]->parentWidget()))->help(tag);  // ахтунг, быдлокод!
   }

  return false;
 }

void RotatingCircleItem :: setHelpTag(QGraphicsItem* item, QString tag)
 {
  item->setAcceptHoverEvents(true);         // assigning help string and installing event handler on item
  item->installSceneEventFilter(this);      // to detect hover event
  item->setData(0, tag);
 }


/* =========================== ASTRO MAP SHOW ======================================= */

Chart :: Chart(QWidget *parent) : AstroFileHandler(parent)
 {
  emptyScene = true;
  mapRect = QRect(-250,-250, 500, 500);
  zoom = 0.8;

  view = new QGraphicsView(this);

  view->setScene(new QGraphicsScene());
  view->setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);
  view->installEventFilter(this);
  view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QVBoxLayout* layout = new QVBoxLayout(this);
   layout->setMargin(0);
   layout->addWidget(view);
 }

void Chart :: fitInView()
 {
  QRect rect(mapRect.x() / zoom, mapRect.y() / zoom, mapRect.width() / zoom, mapRect.height() / zoom);
  view->fitInView(rect, Qt::KeepAspectRatio);
 }

void Chart :: createScene()
 {
  qDebug() << "Create scene";
  QGraphicsScene* s = view->scene();

  QBrush background(QColor(8, 103, 192, 50));
  QPen penZodiac(QColor(31,52,93), zodiacWidth);
  QPen penBorder(QColor(50,145,240));
  QPen penCusp(QColor(227,214,202), 2);
  QPen penCusp0(QColor(250,90,58), 3);
  QPen penCusp10(QColor(210,195,150), 3);
  QPen penCircle(QColor(227,214,202), 1);
  QPen penPlanetMarkers(QColor(255,255,255), 1);
  QFont font("Times", 13, QFont::Bold);
  QFont zodiacFont("Almagest", 16, QFont::Bold);
  QFont planetFont("Almagest", 19, QFont::Bold);
  QFont planetFontSmall("Almagest", 16);
  QColor planetColor = "#cee1f2";
  QColor planetShapeColor = "#78a895";
  QColor signFillColor = Qt::black;
  QColor signShapeColor = "#6d6d6d";

  if (coloredZodiac)
   {
    QConicalGradient grad1(mapRect.center(), 180);
    QColor color;

    foreach (const A::ZodiacSign& sign, file()->horoscope().zodiac.signs)
     {
      color.setNamedColor(sign.userData["bgcolor"].toString());
      float a1 = sign.startAngle / 360;
      float a2 = sign.endAngle / 360 - 0.0001;

      if (clockwise)
       {
        a1 = 0.5 - a1; if (a1 < 0) a1 += 1;
        a2 = 0.5 - a2; if (a2 < 0) a2 += 1;
       }

      grad1.setColorAt(a1, color);
      grad1.setColorAt(a2, color);
     }

    penZodiac.setBrush(QBrush(grad1));
    penBorder.setColor(Qt::black);

   }


  for (int i = 0; i < 12; i++)        // cuspides
   {
    if (i == 0)
      cuspides << s->addLine(-innerRadius - penCircle.width(), 0,
                             mapRect.x() - cuspideLength * 1.4, 0, penCusp0);
    else if (i == 9)
      cuspides << s->addLine(-innerRadius - penCircle.width(), 0,
                             mapRect.x() - cuspideLength * 1.4, 0, penCusp10);
    else
      cuspides << s->addLine(-innerRadius - penCircle.width(), 0, mapRect.x() - cuspideLength, 0, penCusp);

    cuspideLabels << s->addSimpleText(A::romanNum(i+1), font);
    cuspideLabels.last()->setBrush(QColor(255,255,255,150));
    cuspideLabels.last()->setParentItem(cuspides[i]);
    cuspideLabels.last()->moveBy(mapRect.x() - cuspideLength + 5, 5);
    cuspideLabels.last()->setTransformOriginPoint(cuspideLabels.last()->boundingRect().center());
   }

  QPointF p = cuspides[0]->line().p2();
  s->addLine(p.x(), p.y(), p.x() + 14, p.y() + 7, penCusp0)->setParentItem(cuspides[0]);  // arrow for first cuspide
  s->addLine(p.x(), p.y(), p.x() + 14, p.y() - 7, penCusp0)->setParentItem(cuspides[0]);


  s->addEllipse(-innerRadius, -innerRadius, 2 * innerRadius,
                2 * innerRadius, penCircle);                                // inner circle
  s->addEllipse(mapRect.adjusted(2,2,-2,-2), penBorder, background);        // fill background (with margin)

  circle = new RotatingCircleItem(mapRect, penZodiac);                      // zodiac circle
  circle->setCursor(QPixmap("chart/rotate.png"));
  s->addItem(circle);
  s->addEllipse(mapRect, penBorder)->setParentItem(circle);                 // zodiac outer border
  s->addEllipse(mapRect.adjusted(zodiacWidth, zodiacWidth,                  // zodiac inner border
                                 -zodiacWidth, -zodiacWidth), penBorder)->setParentItem(circle);

  if (zodiacDropShadow)
   {
    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect;
    effect->setBlurRadius(zodiacWidth);
    effect->setOffset(0);
    effect->setColor(QColor(0,0,0,150));
    circle->setGraphicsEffect(effect);
   }

  foreach (const A::ZodiacSign& sign, file()->horoscope().zodiac.signs)
   {
    float endAngle = sign.endAngle;
    if (sign.startAngle > endAngle) endAngle += 360;
    float rad = -sign.startAngle * 3.1416 / 180;
    float rad_mid = -(sign.startAngle + (endAngle - sign.startAngle) / 2) * 3.1416 / 180;

    if (clockwise)
     {
      rad     = 3.1416 - rad;
      rad_mid = 3.1416 - rad_mid;
     }

    s->addLine( mapRect.x() * cos(rad),                               // zodiac sign borders
                mapRect.y() * sin(rad),
               (mapRect.x() + zodiacWidth) * cos(rad),
               (mapRect.y() + zodiacWidth) * sin(rad), penBorder)->setParentItem(circle);

    QString ch = QString(sign.userData["fontChar"].toInt());
    QGraphicsSimpleTextItem* text = s->addSimpleText(ch, zodiacFont); // zodiac sign icon
    text->setParentItem(circle);
    text->setBrush(coloredZodiac ? signFillColor  : sign.userData["fillColor"].toString());
    text->setPen  (coloredZodiac ? signShapeColor : sign.userData["shapeColor"].toString());
    text->setOpacity(0.9);
    text->moveBy((mapRect.x() + zodiacWidth / 2) * cos(rad_mid) - text->boundingRect().width() / 2,
                 (mapRect.y() + zodiacWidth / 2) * sin(rad_mid) - text->boundingRect().height() / 2);
    text->setTransformOriginPoint(text->boundingRect().center());
    circle->setHelpTag(text, sign.name);
    signIcons << text;
   }


  foreach(const A::Planet& planet, file()->horoscope().planets)       // planets
   {
    int radius = 4;

    int charIndex = planet.userData["fontChar"].toInt();

    QColor color (planet.userData["color"].toString());
    if (!color.isValid()) color = planetColor;

    QColor shapeColor (planet.userData["shapeColor"].toString());
    if (!shapeColor.isValid()) shapeColor = planetShapeColor;

    QGraphicsSimpleTextItem* text = s->addSimpleText(QString(charIndex),
                                                     planet.isReal ? planetFont : planetFontSmall);

    QGraphicsEllipseItem* marker = s->addEllipse(-innerRadius - radius / 2, -radius / 2,
                                               radius, radius, penPlanetMarkers);

    text   -> setPos(normalPlanetPosX(text, marker), -text->boundingRect().height() / 2);
    text   -> setBrush(color);
    text   -> setPen(shapeColor);
    //text   -> setOpacity(opacity);
    text   -> setTransformOriginPoint(text->boundingRect().center());
    text   -> setParentItem(marker);
    marker -> setTransformOriginPoint (circle->boundingRect().center());
    marker -> setZValue(1);

    planets[planet.id]       = text;
    planetMarkers[planet.id] = marker;
   }


  fitInView();
  emptyScene = false;
 }

void Chart :: updateScene()
 {
  float rotate;

  if (circleStart == Start_Ascendent)
    rotate = file()->horoscope().houses.cusp[0];
  else
    rotate = file()->horoscope().zodiac.signs[0].startAngle;
    if (clockwise) rotate = -rotate;

  circle->setRotation(rotate);

  for (int i = 0; i < 12; i++)                           // update cuspides && labels
   {
    float cusp = file()->horoscope().houses.cusp[i];
    if (clockwise) cusp = 180 - cusp;

    cuspides[i]->setRotation(-cusp + rotate);
    cuspideLabels[i]->setRotation(cusp - rotate);
    signIcons[i]->setRotation(-rotate);

    QString tag = tr("%1+%2").arg(A::romanNum(i+1))
                             .arg(A::getSign(cusp, file()->horoscope().zodiac).name);
    circle->setHelpTag(cuspides[i], tag);
    circle->setHelpTag(cuspideLabels[i], tag);

    cuspides[i]->setToolTip(tr("House %1\n%2").arg(A::romanNum(i+1))
                                              .arg(A::zodiacPosition(cusp, file()->horoscope().zodiac)));
    cuspideLabels[i]->setToolTip(cuspides[i]->toolTip());
   }


  foreach (const A::Planet& p, file()->horoscope().planets)  // update planets
   {
    QGraphicsItem* marker = planetMarkers[p.id];
    QGraphicsItem* planet = planets[p.id];
    float angle = p.eclipticPos.x();
    if (clockwise) angle = 180 - angle;

    planet -> setPos(normalPlanetPosX(planet, marker),planet->pos().y());
    planet -> setRotation(angle - rotate);
    marker -> setRotation(rotate - angle);

    foreach (QGraphicsItem* other, planets)              // avoid intersection of planets
     {
      if (other == planet) break;
      if (qAbs(planet->rotation() - other->rotation()) < 10)
        planet->moveBy(-other->boundingRect().width(), 0);
     }

    QString toolTip = QString("%1 %2, %3").arg(p.name)
                                          .arg(A::zodiacPosition(p, file()->horoscope().zodiac, A::HighPrecision))
                                          .arg(A::houseNum(p));
    planet -> setToolTip(toolTip);
    marker -> setToolTip(toolTip);
    circle->setHelpTag(planet, p.name + "+" + p.sign->name);
    circle->setHelpTag(marker, p.name);
   }


  int i = 0;
  foreach (const A::Aspect& asp, file()->horoscope().aspects)   // update aspects
   {
    QLineF line(planetMarkers[asp.planet1->id]->sceneBoundingRect().center(),
                planetMarkers[asp.planet2->id]->sceneBoundingRect().center());

    if (i >= aspects.count())                 // add or change geometry
      aspects << view->scene()->addLine(line, aspectPen(asp));
    else
     {
      aspects[i]->setLine(line);
      aspects[i]->setPen(aspectPen(asp));
     }


    QString toolTip = A::describeAspect(asp);
    //int penWidth = 2;
    //QColor color;

    //if (qAbs(A::getAspect(asp.id).angle - asp.angle) > asp.orbise / 2) // inaccuracy aspect
    //  penWidth = 1;

    //color.setNamedColor(asp.d->userData["color"].toString());
    //if (!color.isValid()) color = Qt::blue;


    if (aspects[i]->toolTip() != toolTip)     // assign messages
     {
      aspects[i]->setToolTip(toolTip);
      circle->setHelpTag(aspects[i], QString("%1+%2+%3").arg(asp.d->name)
                                                        .arg(asp.planet1->name)
                                                        .arg(asp.planet2->name));
     }

    /*if (aspects[i]->pen().color() != color ||
        aspects[i]->pen().width() != penWidth)   // assign paint parameters
     {
      aspects[i]->setPen(QPen(color, penWidth));
     }*/

    //aspects[i]->setOpacity(1);
    /*if (!file()->horoscope().planets[asp.planet1].isReal)
      aspects[i]->setOpacity(aspects[i]->opacity() / 2);

    if (!file()->horoscope().planets[asp.planet2].isReal)
      aspects[i]->setOpacity(aspects[i]->opacity() / 2);*/

    i++;
   }

  while (i != aspects.count())     // remove unused aspect items
   {
    view->scene()->removeItem(aspects.takeLast());
    //aspectMarkers.removeLast();
   }

 }

int  Chart :: normalPlanetPosX(QGraphicsItem* planet, QGraphicsItem* marker)
 {
  int indent = 6;
  return marker->boundingRect().x() - planet->boundingRect().width() - indent;
 }

const QPen& Chart :: aspectPen(const A::Aspect& asp)
 {
  static QMap<QString, QPen>pens;
  if (pens.isEmpty())
   {
    pens["--"] = QPen(QBrush(QColor(207,41,33)),  2);
    pens["-"]  = QPen(QBrush(QColor(230,155,57)), 2);
    pens["0"]  = QPen(QBrush(QColor(15,114,248)), 2);
    pens["+"]  = QPen(QBrush(QColor(14,162,98)),  2);
    pens["++"] = QPen(QBrush(QColor(77,206,113)), 2);
   }

  QString tag = asp.d->userData["good"].toString();
  if (pens.contains(tag))
    return pens[tag];

  return pens["0"];
 }

void Chart :: clearScene()
 {
  view->scene()->clear();
  emptyScene = true;
  cuspides.clear();
  cuspideLabels.clear();
  planets.clear();
  planetMarkers.clear();
  aspects.clear();
  aspectMarkers.clear();
  signIcons.clear();
 }

void Chart :: fileUpdated(AstroFile::Members members)
 {
  if (file()->isEmpty()) return;
  if (members & AstroFile::Zodiac) clearScene();
  if (emptyScene) createScene();
  circle->setFile(file());
  if (members & (AstroFile::GMT | AstroFile::Timezone | AstroFile::Location |
                 AstroFile::HouseSystem | AstroFile::AspectLevel | AstroFile::Zodiac))
    updateScene();
 }

void Chart :: resizeEvent (QResizeEvent*)
 {
  fitInView();
 }

AppSettings Chart :: defaultSettings ()
 {
  AppSettings s;
  s.setValue ( "Circle/circleStart",      Start_Ascendent );
  s.setValue ( "Circle/clockwise",        false );
  s.setValue ( "Circle/zodiacWidth",      37 );
  s.setValue ( "Circle/cuspideLength",    33 );
  s.setValue ( "Circle/innerRadius",      100 );
  s.setValue ( "Circle/coloredZodiac",    true );
  s.setValue ( "Circle/zodiacDropShadow", true );
  return s;
 }

AppSettings Chart :: currentSettings ()
 {
  AppSettings s;
  s.setValue ( "Circle/circleStart",      circleStart );
  s.setValue ( "Circle/clockwise",        clockwise );
  s.setValue ( "Circle/zodiacWidth",      zodiacWidth );
  s.setValue ( "Circle/cuspideLength",    cuspideLength );
  s.setValue ( "Circle/innerRadius",      innerRadius );
  s.setValue ( "Circle/coloredZodiac",    coloredZodiac );
  s.setValue ( "Circle/zodiacDropShadow", zodiacDropShadow );
  return s;
 }

void Chart :: applySettings      ( const AppSettings& s )
 {
  circleStart      = (CircleStart)s.value("Circle/circleStart").toInt();
  clockwise        = s.value ( "Circle/clockwise"        ).toBool();
  zodiacWidth      = s.value ( "Circle/zodiacWidth"      ).toInt();
  cuspideLength    = s.value ( "Circle/cuspideLength"    ).toInt();
  innerRadius      = s.value ( "Circle/innerRadius"      ).toInt();
  coloredZodiac    = s.value ( "Circle/coloredZodiac"    ).toBool();
  zodiacDropShadow = s.value ( "Circle/zodiacDropShadow" ).toBool();

  if (!emptyScene)
   {
    clearScene();
    fileUpdated(AstroFile::All);    // redraw all
   }
 }

void Chart :: setupSettingsEditor ( AppSettingsEditor* ed )
 {
  ed -> addTab (tr("Chart"));

  QMap<QString, QVariant> values;
  values[tr("Ascendent")] = Start_Ascendent;
  values[tr("0 Aries")]   = Start_ZeroDegree;
  ed -> addComboBox("Circle/circleStart",      tr("Circle start:"), values);

  ed -> addCheckBox("Circle/clockwise",        tr("Clockwise circle"));
  ed -> addSpinBox ("Circle/zodiacWidth",      tr("Zodiac circle width:"), 5,  1000);
  ed -> addSpinBox ("Circle/cuspideLength",    tr("Cusp line length"),     0,  1000);
  ed -> addSpinBox ("Circle/innerRadius",      tr("Inner circle:"),        10, 1000);
  ed -> addSpacing (10);
  ed -> addControl ("Circle/coloredZodiac",    tr("Colored circle:"));
  ed -> addControl ("Circle/zodiacDropShadow", tr("Drop shadow:"));
 }