#include "drc.h"

bool comp(std::pair<int, point_2d> a, std::pair<int, point_2d> b)
{
    return a.second < b.second;
}

void Drc::createRTree()
{
    m_rtrees.resize(m_db.getNumCopperLayers());
    std::vector<net> nets = m_db.getNets();
    int id = 0;
    for (auto &&net : nets)
    {
        std::vector<Pin> pins = net.getPins();
        std::vector<Segment> segments = net.getSegments();
        std::vector<Via> vias = net.getVias();

        for (auto &&s : segments)
        {
            points_2d line = s.getPos();
            auto width = s.getWidth();
            //std::cout << "width: " << width << std::endl;
            int layerId = m_db.getLayerId(s.getLayer());
            if (layerId == 31)
                continue;
            points_2d coord = segmentToOctagon(line, width, 0.1);
            //std::cout << "Segment: (" << line[0].m_x << "," << line[0].m_y << ")" << "(" << line[1].m_x << "," << line[1].m_y << ")" << std::endl;
            polygon_t polygon;
            //std::cout << "Polygon(";
            for (auto &&p : coord)
            {
                //    std::cout << "(" << p.m_x << "," << p.m_y << "),";
                double x = p.m_x;
                double y = p.m_y;
                bg::append(polygon.outer(), point(x, y));
            }
            // std::cout << std::endl;
            bg::append(polygon.outer(), point(coord[0].m_x, coord[0].m_y));

            box b = bg::return_envelope<box>(polygon);
            Object obj(ObjectType::SEGMENT, s.getId(), s.getNetId(), -1, -1);
            obj.setShape(coord);
            obj.setPoly(polygon);
            obj.setBBox(b);
            obj.setPos(s.getPos());
            if (layerId == 0)
            {
                m_rtrees[0].insert(std::make_pair(b, id));
                obj.setRTreeId(std::make_pair(0, id));
            }
            else if (layerId == 31)
            {
                m_rtrees[1].insert(std::make_pair(b, id));
                obj.setRTreeId(std::make_pair(1, id));
            }
            m_objects.push_back(obj);
            ++id;
        }

        for (auto &&v : vias)
        {
            point_2d pos = v.getPos();
            std::cout << "Via: " << pos.m_x << "," << pos.m_y << std::endl;
            auto size = v.getSize();
            points_2d coord = viaToOctagon(size, pos, 0.1);
            polygon_t polygon;
            std::cout << "Polygon(";
            for (auto &&p : coord)
            {
                std::cout << "(" << p.m_x << "," << p.m_y << "),";
                double x = p.m_x;
                double y = p.m_y;
                bg::append(polygon.outer(), point(x, y));
            }
            std::cout << std::endl;
            bg::append(polygon.outer(), point(coord[0].m_x, coord[0].m_y));

            box b = bg::return_envelope<box>(polygon);
            Object obj(ObjectType::VIA, v.getId(), v.getNetId(), -1, -1);

            obj.setShape(coord);
            obj.setPoly(polygon);
            obj.setBBox(b);
            obj.setPos(pos);
            m_rtrees[0].insert(std::make_pair(b, id));
            obj.setRTreeId(std::make_pair(0, id));
            m_rtrees[1].insert(std::make_pair(b, id));
            obj.setRTreeId(std::make_pair(1, id));
            ++id;
            m_objects.push_back(obj);
        }

        for (auto &&pin : pins)
        {
            int padId = pin.getPadstackId();
            int compId = pin.getCompId();
            int instId = pin.getInstId();
            polygon_t polygon;
            component comp = m_db.getComponent(compId);
            instance inst = m_db.getInstance(instId);
            auto &pad = comp.getPadstack(padId);
            auto pinPos = point_2d{};
            m_db.getPinPosition(pin, &pinPos);
            points_2d coord = pinShapeToOctagon(pad.getSize(), pad.getPos(), 0.1, inst.getAngle(), pad.getAngle());
            std::cout << "net: " << net.getId() << " comp Name: " << comp.getName() << " inst Name: " << inst.getName() << " pad: " << pad.getName() << std::endl;
            std::cout << " \tinst pos: (" << inst.getX() << "," << inst.getY() << ")" << std::endl;
            std::cout << " \tpin pos: (" << pinPos.m_x << "," << pinPos.m_y << ")" << std::endl;
            for (auto &&p : coord)
            {
                p.m_x = p.m_x + pinPos.m_x;
                p.m_y = p.m_y + pinPos.m_y;
            }

            std::cout << "Polygon(";
            for (auto &&p : coord)
            {
                std::cout << "(" << p.m_x << "," << p.m_y << "),";
                double x = p.m_x;
                double y = p.m_y;
                bg::append(polygon.outer(), point(x, y));
            }
            std::cout << std::endl;
            bg::append(polygon.outer(), point(coord[0].m_x, coord[0].m_y));
            std::vector<int> layers = m_db.getPinLayer(instId, padId);
            box b = bg::return_envelope<box>(polygon);
            Object obj(ObjectType::PIN, pad.getId(), net.getId(), compId, instId);
            obj.setPos(pinPos);
            obj.setShape(coord);
            obj.setPoly(polygon);
            obj.setBBox(b);

            for (auto &&layer : layers)
            {
                if (layer == 0)
                {
                    m_rtrees[0].insert(std::make_pair(b, id));
                    obj.setRTreeId(std::make_pair(0, id));
                }
                else if (layer == 31)
                {
                    m_rtrees[1].insert(std::make_pair(b, id));
                    obj.setRTreeId(std::make_pair(1, id));
                }
            }
            m_objects.push_back(obj);
            ++id;
        }
    }

    std::vector<Pin> pins = m_db.getUnconnectedPins();
    for (auto &&pin : pins)
    {
        int padId = pin.getPadstackId();
        int compId = pin.getCompId();
        int instId = pin.getInstId();
        polygon_t polygon;
        component comp = m_db.getComponent(compId);
        instance inst = m_db.getInstance(instId);
        auto &pad = comp.getPadstack(padId);
        points_2d coord = pinShapeToOctagon(pad.getSize(), pad.getPos(), 0.1, inst.getAngle(), pad.getAngle());
        auto pinPos = point_2d{};
        m_db.getPinPosition(pin, &pinPos);

        for (auto &&p : coord)
        {
            p.m_x = p.m_x + pinPos.m_x;
            p.m_y = p.m_y + pinPos.m_y;
        }

        for (auto &&p : coord)
        {
            double x = p.m_x;
            double y = p.m_y;
            bg::append(polygon.outer(), point(x, y));
        }
        bg::append(polygon.outer(), point(coord[0].m_x, coord[0].m_y));
        std::vector<int> layers = m_db.getPinLayer(instId, padId);
        box b = bg::return_envelope<box>(polygon);
        Object obj(ObjectType::PIN, pad.getId(), -1, compId, instId);
        obj.setPos(pinPos);
        obj.setShape(coord);
        obj.setPoly(polygon);
        obj.setBBox(b);

        for (auto &&layer : layers)
        {
            if (layer == 0)
            {
                m_rtrees[0].insert(std::make_pair(b, id));
                obj.setRTreeId(std::make_pair(0, id));
            }
            else if (layer == 31)
            {
                m_rtrees[1].insert(std::make_pair(b, id));
                obj.setRTreeId(std::make_pair(1, id));
            }
        }
        m_objects.push_back(obj);
        ++id;
    }
}
/*
bool Drc::checkIntersection()
{
    
    

    return true;
}*/

points_2d Drc::buildRelation(int &obj1Id, const int &obj2Id)
{
    auto &&obj1 = m_objects[obj1Id];
    auto &&obj2 = m_objects[obj2Id];
    auto &&shape1 = obj1.getShape();
    auto &&shape2 = obj2.getShape();
    point_2d center;
    points_2d pos = obj1.getPos();
    points_2d proCoord1, proCoord2, coord;
    ;
    if (obj1.getType() == ObjectType::SEGMENT)
    {
        center.m_x = (pos[0].m_x + pos[1].m_x) / 2;
        center.m_y = (pos[1].m_y + pos[1].m_y) / 2;
    }
    else
    {
        center = pos[0];
    }

    std::vector<std::vector<std::pair<int, point_2d>>> fourCoods;
    double dist = 10000000;
    bool overlap = true;
    std::vector<std::pair<int, point_2d>> proj1, proj2;

    for (int i = 0; i < 4; ++i)
    {
        proCoord1 = projection(center, shape1, i * 45);
        proCoord2 = projection(center, shape2, i * 45);

        std::vector<std::pair<int, point_2d>> projectionVec;
        std::vector<std::pair<int, point_2d>> project1, project2;

        for (int i = 0; i < proCoord1.size(); ++i)
        {
            int id = 10 + i;
            projectionVec.push_back(std::make_pair(id, proCoord1[i]));
            project1.push_back(std::make_pair(i, proCoord1[i]));
        }
        for (int i = 0; i < proCoord2.size(); ++i)
        {
            int id = 20 + i;
            projectionVec.push_back(std::make_pair(id, proCoord2[i]));
            project2.push_back(std::make_pair(i, proCoord2[i]));
        }
        std::sort(projectionVec.begin(), projectionVec.end(), comp);
        std::sort(project1.begin(), project1.end(), comp);
        std::sort(project2.begin(), project2.end(), comp);

        // Case1:
        //    obj1         obj2
        // *--------*
        //        *---------:intersect
        if (project1[0].second < project2[0].second && project2[0].second < project1[7].second &&
            project1[7].second < project2[7].second)
        {
            dist = project1[7].second.getDistance(project1[7].second, project2[0].second);
            overlap = true;
        }
        else if (project2[0].second < project1[0].second && project1[0].second < project2[7].second &&
                 project2[7].second < project1[7].second)
        {
            dist = project2[7].second.getDistance(project2[7].second, project1[0].second);
            overlap = true;
        }
        // Case2:
        //
        // *--------*   *---------*:sep
        else if (project1[7].second < project2[0].second)
        {
            dist = project1[7].second.getDistance(project1[7].second, project2[0].second);
            overlap = false;
        }
        else if (project2[7].second < project1[0].second)
        {
            dist = project1[7].second.getDistance(project1[7].second, project2[0].second);
            overlap = false;
        }
        // Case3:
        //     *-----*
        // *-------------*
        else if (project1[0].second < project2[0].second && project2[0].second < project1[7].second && project1[0].second < project2[7].second && project2[7].second < project1[7].second)
        {
            dist = project2[0].second.getDistance(project2[0].second, project2[7].second);
            overlap = true;
        }
        else if (project2[0].second < project1[0].second && project1[0].second < project2[7].second && project2[0] < project1[7] &&
                 project1[7].second < project2[7].second)
        {
            dist = project1[0].second.getDistance(project1[0].second, project1[7].second);
            overlap = true;
        }

        /*
        std::cout << "degree :" << i*45 << std::endl;
        int obj1L, obj1R, obj2L, obj2R;
        bool obj1SepObj2 = false, obj2SepObj1 = false, obj1interObj2 = false,
             obj2interObj1 = false, obj1ConObj2 = false, obj2ConObj1 = false;

        // *---------*
        //       *----------*:inter
        //        *----*
        //  *----------------*:con
        int count1 = 0, count2 = 0;

        for (int j = 0 ; j < projectionVec.size(); ++j) {
        //    std::cout << projectionVec[j].first << std::endl;
        //    printPoint(projectionVec[j].second);
            if(projectionVec[j].first < 20) {
                ++count1;
                if(count1 == 1) obj1L = projectionVec[j].first;
                else if(count1 == 8 && count2 == 0) {
                    obj1R = projectionVec[j].first;
                    obj1SepObj2 = true;
                } 
            } else {
                 ++count2;
                if(count2 == 1) obj2L = projectionVec[j].first;
                else if(count2 == 8 && count1 == 0) {
                    obj2R = projectionVec[j].first;
                    obj2SepObj1 = true;
                }
            }
        }*/

        return coord;
    }
}

void Drc::traverseRTree()
{
    std::cout << "=========================" << std::endl;
    double MAX_DIST = 0.2;
    bg::model::point<double, 2, bg::cs::cartesian> point1;
    bg::model::point<double, 2, bg::cs::cartesian> point2;
    //for(auto &&obj1 : m_objects) {
    auto &&obj1 = m_objects[191];
    if (obj1.getType() == ObjectType::PIN)
    {
        auto compId = obj1.getCompId();
        auto instId = obj1.getInstId();
        auto padId = obj1.getDBId();
        component comp = m_db.getComponent(compId);
        instance inst = m_db.getInstance(instId);
        auto &pad = comp.getPadstack(padId);
        std::cout << "comp: " << comp.getName() << " inst: " << inst.getName();
        std::cout << " pad: " << pad.getName() << std::endl;
    }
    auto bbox = obj1.getBBox();
    std::vector<std::pair<int, int>> &rtreeId = obj1.getRtreeId();
    int rId = rtreeId[0].first;
    std::cout << "Object Id: " << rtreeId[0].first << "," << rtreeId[0].second << std::endl;
    auto coord = obj1.getShape();
    printPolygon(coord);

    double minX = bg::get<bg::min_corner, 0>(bbox);
    double minY = bg::get<bg::min_corner, 1>(bbox);
    double maxX = bg::get<bg::max_corner, 0>(bbox);
    double maxY = bg::get<bg::max_corner, 1>(bbox);
    minX -= MAX_DIST;
    minY -= MAX_DIST;
    maxX += MAX_DIST;
    maxY += MAX_DIST;
    point1.set<0>(minX);
    point1.set<1>(minY);
    point2.set<0>(maxX);
    point2.set<1>(maxY);
    box query_box(point1, point2);
    std::vector<value> result_s;
    m_rtrees[rId].query(bgi::intersects(query_box), std::back_inserter(result_s));

    std::cout << "spatial query box:" << std::endl;
    std::cout << bg::wkt<box>(query_box) << std::endl;
    std::cout << "spatial query result:" << std::endl;
    BOOST_FOREACH (value const &v, result_s)
    {
        std::cout << bg::wkt<box>(v.first) << " - " << v.second << std::endl;

        auto &&obj2 = m_objects[v.second];
        std::deque<polygon_t> output;
        polygon_t poly1 = obj1.getPoly();
        polygon_t poly2 = obj2.getPoly();
        std::vector<std::pair<int, int>> &rtreeId2 = obj2.getRtreeId();
        std::cout << "Object2 Id: " << rtreeId2[0].first << "," << rtreeId2[0].second << std::endl;
        auto coord = obj2.getShape();
        printPolygon(coord);

        if (obj1.getNetId() != obj2.getNetId())
            buildRelation(rtreeId[0].second, v.second);

        /*std::cout << "!!Overlap!!" << std::endl;
            if (boost::geometry::intersects(poly1, poly2)) {
                boost::geometry::intersection(poly1, poly2, output);
                BOOST_FOREACH(polygon_t const& p, output)
                {
                    std::cout << "\tarea:" << boost::geometry::area(p) << std::endl;
                    std::cout << " obj1 id: " << obj1.getId() << ", obj2 id: " << obj2.getId() << std::endl;
                }

                if (obj2.getType() == ObjectType::SEGMENT) {
                    auto dbId = obj2.getDBId();
                    points_2d pos = obj2.getPos();
                    std::cout << "segment: ";
                    for(auto &&p : pos){ 
                        std::cout << "(" << p.m_x << "," << p.m_y << ") ";
                        
                    }
                    std::cout << std::endl;

                    std::cout << "Polygon(";
                    auto coord = obj2.getShape();
                    for(auto &&p : coord)
                    {
                        std::cout << "(" << p.m_x << "," << p.m_y << "),";

                    }
                    std::cout << std::endl;
                }
            }*/
    }

    /*auto center = point_2d{0,0};
        for (auto &&p : obj1.getShape())
        {
            center.m_x += p.m_x;
            center.m_y += p.m_y;
        }   */
    //}
}

points_2d Drc::projection(point_2d &center, points_2d &octagon, int degree)
{
    auto coord = points_2d{};
    if (degree == 0)
    {
        for (auto &&p : octagon)
        {
            coord.push_back(point_2d{p.m_x, center.m_y});
        }
        return coord;
    }
    else if (degree == 45)
    {
        double b1 = center.m_y - center.m_x;
        for (auto &&p : octagon)
        {
            double b2 = p.m_x + p.m_y;
            auto point = point_2d{};
            point.m_x = 0.5 * (b2 - b1);
            point.m_y = 0.5 * (b1 + b2);
            coord.push_back(point);
        }
        return coord;
    }
    else if (degree == 90)
    {
        for (auto &&p : octagon)
        {
            coord.push_back(point_2d{center.m_x, p.m_y});
        }
        return coord;
    }
    else if (degree == 135)
    {
        double b1 = center.m_x + center.m_y;
        for (auto &&p : octagon)
        {
            double b2 = p.m_y - p.m_x;
            auto point = point_2d{};
            point.m_x = 0.5 * (b1 - b2);
            point.m_y = 0.5 * (b1 + b2);
            coord.push_back(point);
        }
        return coord;
    }
}

void Drc::testProjection()
{
    for (auto &&obj1 : m_objects)
    {
        for (auto &&obj2 : m_objects)
        {
            auto coord1 = obj1.getShape();
            auto coord2 = obj2.getShape();
            auto cen = point_2d{0, 0};

            for (size_t i = 0; i < 8; ++i)
            {
                cen.m_x += coord1[i].m_x;
                cen.m_y += coord1[i].m_y;
            }

            cen.m_x = cen.m_x / 8;
            cen.m_y = cen.m_y / 8;
            auto p1 = projection(cen, coord1, 0);
            auto p2 = projection(cen, coord2, 0);
            std::cout << "=================================" << std::endl;

            std::cout << "Polygon(";
            for (auto &&p : coord1)
            {
                std::cout << "(" << p.m_x << "," << p.m_y << "),";
            }
            std::cout << std::endl;
            for (auto &&p : p1)
            {
                std::cout << "Point({" << p.m_x << "," << p.m_y << "})" << std::endl;
            }

            std::cout << "Polygon(";
            for (auto &&p : coord2)
            {
                std::cout << "(" << p.m_x << "," << p.m_y << "),";
            }
            std::cout << std::endl;
            for (auto &&p : p2)
            {
                std::cout << "Point({" << p.m_x << "," << p.m_y << "})" << std::endl;
            }
        }
    }
}

/*
void Drc::checkClearance()
{
    if (obj1.getType() == ObjectType::PIN){
        auto compId = obj1.getCompId();
        auto instId = obj1.getInstId();
        auto padId = obj1.getDBId();
        component comp = m_db.getComponent(compId);
        instance inst = m_db.getInstance(instId);
        auto &pad = comp.getPadstack(padId);
        std::cout << "comp: " << comp.getName() << " inst: " << inst.getName();
        std::cout << " pad: " << pad.getName() << std::endl;
    }
        auto bbox = obj1.getBBox();
        std::vector< std::pair<int,int> > & rtreeId = obj1.getRtreeId();
        int rId = rtreeId[0].first;
        std::cout << "Object Id: " << rtreeId[0].first << "," << rtreeId[0].second << std::endl;
        auto coord = obj1.getShape();
        printPolygon(coord);

        double minX = bg::get<bg::min_corner, 0>(bbox);
        double minY = bg::get<bg::min_corner, 1>(bbox);
        double maxX = bg::get<bg::max_corner, 0>(bbox);
        double maxY = bg::get<bg::max_corner, 1>(bbox);
        minX -= MAX_DIST;
        minY -= MAX_DIST;
        maxX += MAX_DIST;
        maxY += MAX_DIST;
        point1.set<0>(minX);
        point1.set<1>(minY);
        point2.set<0>(maxX);
        point2.set<1>(maxY);
        box query_box(point1, point2);
        std::vector<value> result_s;
        m_rtrees[rId].query(bgi::intersects(query_box), std::back_inserter(result_s));


        std::cout << "spatial query box:" << std::endl;
        std::cout << bg::wkt<box>(query_box) << std::endl;
        std::cout << "spatial query result:" << std::endl;
        BOOST_FOREACH(value const& v, result_s) {
            std::cout << bg::wkt<box>(v.first) << " - " << v.second << std::endl;
            

            auto &&obj2 = m_objects[v.second];
            std::deque<polygon_t> output;
            polygon_t poly1 = obj1.getPoly();
            polygon_t poly2 = obj2.getPoly();
            std::vector< std::pair<int,int> > & rtreeId2 = obj2.getRtreeId();
            std::cout << "Object2 Id: " << rtreeId2[0].first << "," << rtreeId2[0].second << std::endl;
                auto coord = obj2.getShape();
                printPolygon(coord);

            if (obj1.getNetId() != obj2.getNetId())
                buildRelation(rtreeId[0].second, v.second);

            /*std::cout << "!!Overlap!!" << std::endl;
            if (boost::geometry::intersects(poly1, poly2)) {
                boost::geometry::intersection(poly1, poly2, output);
                BOOST_FOREACH(polygon_t const& p, output)
                {
                    std::cout << "\tarea:" << boost::geometry::area(p) << std::endl;
                    std::cout << " obj1 id: " << obj1.getId() << ", obj2 id: " << obj2.getId() << std::endl;
                }*/

//}

void Drc::printDrc()
{
    int count = 0;
    for (auto &&obj1 : m_objects)
    {
        for (auto &&obj2 : m_objects)
        {
            if (obj1.getNetId() == obj2.getNetId())
                continue;
            if (obj1.getType() == ObjectType::PIN && obj2.getType() == ObjectType::PIN && obj1.getInstId() == obj2.getInstId())
                continue;
            std::deque<polygon_t> output;
            polygon_t blue = obj1.getPoly();
            polygon_t green = obj2.getPoly();

            //bool b = boost::geometry::intersects(blue, green);
            //std::cout << b << std::endl;
            /*
            int i = 0;
            BOOST_FOREACH(polygon_t const& p, output)
            {
                std::cout << i++ << ": " << boost::geometry::area(p) << std::endl;
            }
            */
            if (boost::geometry::intersects(blue, green))
            { //output.size() != 0) {

                boost::geometry::intersection(green, blue, output);
                BOOST_FOREACH (polygon_t const &p, output)
                {

                    if (boost::geometry::area(p) > 0.05)
                    {
                        count++;
                        std::cout << "Conflict: " << std::endl;
                        std::cout << "\tarea:" << boost::geometry::area(p) << std::endl;
                        std::cout << " obj1 id: " << obj1.getId() << ", obj2 id: " << obj2.getId() << std::endl;
                        if (obj1.getType() == ObjectType::PIN)
                        {
                            auto compId = obj1.getCompId();
                            auto instId = obj1.getInstId();
                            auto padId = obj1.getDBId();
                            component comp = m_db.getComponent(compId);
                            instance inst = m_db.getInstance(instId);
                            auto &pad = comp.getPadstack(padId);
                            std::cout << "comp: " << comp.getName() << " inst: " << inst.getName();
                            std::cout << " pad: " << pad.getName() << std::endl;

                            std::cout << "Polygon(";
                            auto coord = obj1.getShape();
                            for (auto &&p : coord)
                            {
                                std::cout << "(" << p.m_x << "," << p.m_y << "),";
                            }
                            std::cout << std::endl;
                        }
                        else if (obj1.getType() == ObjectType::SEGMENT)
                        {
                            auto dbId = obj1.getDBId();
                            points_2d pos = obj1.getPos();
                            std::cout << "segment: ";
                            for (auto &&p : pos)
                            {
                                std::cout << "(" << p.m_x << "," << p.m_y << ") ";
                            }
                            std::cout << std::endl;

                            std::cout << "Polygon(";
                            auto coord = obj1.getShape();
                            for (auto &&p : coord)
                            {
                                std::cout << "(" << p.m_x << "," << p.m_y << "),";
                            }
                            std::cout << std::endl;
                        }
                        else if (obj1.getType() == ObjectType::VIA)
                        {
                            auto dbId = obj1.getDBId();
                            points_2d pos = obj1.getPos();
                            std::cout << "via: (" << pos[0].m_x << "," << pos[0].m_y << ")" << std::endl;

                            std::cout << "Polygon(";
                            auto coord = obj1.getShape();
                            for (auto &&p : coord)
                            {
                                std::cout << "(" << p.m_x << "," << p.m_y << "),";
                            }
                            std::cout << std::endl;
                        }

                        if (obj2.getType() == ObjectType::PIN)
                        {
                            auto compId = obj2.getCompId();
                            auto instId = obj2.getInstId();
                            auto padId = obj2.getDBId();
                            component comp = m_db.getComponent(compId);
                            instance inst = m_db.getInstance(instId);
                            auto &pad = comp.getPadstack(padId);
                            std::cout << "comp: " << comp.getName() << " inst: " << inst.getName();
                            std::cout << " pad: " << pad.getName() << std::endl;
                            std::cout << " obj id: " << obj2.getId() << std::endl;

                            std::cout << "Polygon(";
                            auto coord = obj2.getShape();
                            for (auto &&p : coord)
                            {
                                std::cout << "(" << p.m_x << "," << p.m_y << "),";
                            }
                            std::cout << std::endl;
                        }
                        else if (obj2.getType() == ObjectType::SEGMENT)
                        {
                            auto dbId = obj2.getDBId();
                            points_2d pos = obj2.getPos();
                            std::cout << "segment: ";
                            for (auto &&p : pos)
                            {
                                std::cout << "(" << p.m_x << "," << p.m_y << ") ";
                            }
                            std::cout << std::endl;

                            std::cout << "Polygon(";
                            auto coord = obj2.getShape();
                            for (auto &&p : coord)
                            {
                                std::cout << "(" << p.m_x << "," << p.m_y << "),";
                            }
                            std::cout << std::endl;
                        }
                        else if (obj2.getType() == ObjectType::VIA)
                        {
                            auto dbId = obj2.getDBId();
                            points_2d pos = obj2.getPos();
                            std::cout << "via: (" << pos[0].m_x << "," << pos[0].m_y << ")" << std::endl;
                            std::cout << "Polygon(";
                            auto coord = obj2.getShape();
                            for (auto &&p : coord)
                            {
                                std::cout << "(" << p.m_x << "," << p.m_y << "),";
                            }
                            std::cout << std::endl;
                        }
                    }
                }
            }
        }
    }

    std::cout << "###############SUMMARY#################" << std::endl;
    std::cout << "DRC count: " << count / 2 << std::endl;
}

void Drc::printObject()
{
    std::cout << std::endl;
    std::cout << "#####################################" << std::endl;
    std::cout << "###                               ###" << std::endl;
    std::cout << "###             OBJ               ###" << std::endl;
    std::cout << "###                               ###" << std::endl;
    std::cout << "#####################################" << std::endl;
    for (auto &&obj : m_objects)
    {
        std::vector<std::pair<int, int>> ids = obj.getRtreeId();
        for (auto &&id : ids)
        {
            std::cout << "(" << id.first << "," << id.second << ") ";
        }
        std::cout << std::endl;
    }
}

/*std::vector<double> Drc::lineEquation(point_2d &p1, point_2d &p2) {
    double y1 = p1.m_y, y2 = p2.m_y, x1 = p1.m_x, x2 = p1.m_x;
    double slope = (y2 - y1)/(x2 - x1);
    double b = y1 - slope*x1;
    std::vector<double> equ;
    equ.push_back(b);
    equ.push_back(slope);
    return equ;
}*/

void Drc::printPolygon(points_2d &coord)
{
    std::cout << "Polygon(";
    for (size_t i = 0; i < coord.size(); ++i)
    {

        std::cout << coord[i];
        if (i != 7)
            std::cout << ", ";
    }
    std::cout << ")" << std::endl;
}

void Drc::printPoint(point_2d &p)
{
    std::cout << "Point({" << p.m_x << "," << p.m_y << "})" << std::endl;
}
