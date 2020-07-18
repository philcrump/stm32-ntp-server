'use strict';

const doc_gnss_ui = document.getElementById("gnss-ui");
const doc_gnss_svs_ui = document.getElementById("gnss-svs-ui");
const doc_ntpd_ui = document.getElementById("ntpd-ui");

var status_svs = [];
var status_gnss = {};
var status_ntpd = {};

const gnss_lookup_letter = {
  0: "G",
  2: "E",
  3: "C",
  6: "R"
}

const gnss_lookup_name = {
  0: "GPS",
  1: "SBAS",
  2: "GAL",
  3: "BDS",
  4: "IMES",
  5: "QZSS",
  6: "GLO"
}

const gnss_lookup_colour = {
  0: "green",
  2: "blue",
  3: "red",
  6: "yellow"
}

function ts_ms_to_string(ts, ts_ms)
{
  return (new Date((ts * 1000) + ts_ms)).toLocaleString();
}

var gnss_ui = {
  view: function()
  {
    return m("div", [
      m("p", m("b", "Status: "), `${status_gnss.fix ? "Locked" : "Searching.."}`),
      m("p", m("b", "Time: "), `${ts_ms_to_string(status_gnss.ts, status_gnss.ts_ms)}`),
      m("p", m("b", "Position: "), `${status_gnss.lat}°, ${status_gnss.lon}°, ${status_gnss.alt}m`),
      m("p", m("b", "SVs: "), `${status_gnss.svs_locked} locked, ${status_gnss.svs_nav} used in soln.`),
      m("p", m("b", "Timepulse Accuracy: "), `${status_gnss.fix ? `+/- ${status_gnss.time_accuracy_ns}ns` : "N/A"}`),
    ]);
  }
}

m.mount(doc_gnss_ui, gnss_ui);


var gnss_svs_ui = {
  view: function()
  {
    return m("table", {id: 'gnss-svs-table'},
      m("tr", [
        m("th", "GNSS"),
        m("th", "SV"),
        m("th", "C/N0"),
        m("th", "Used in soln.")
      ]),
      status_svs.map(function(sv_obj) {
        return m("tr", [
          m("td", gnss_lookup_name[sv_obj.gnss] || "??"),
          m("td", `${gnss_lookup_letter[sv_obj.gnss] || ""}${sv_obj.sv}`),
          m("td", sv_obj.cn0),
          m("td", `${sv_obj.nav ? "Y" : "N"}`)
        ]);
      })
    );
  }
}

m.mount(doc_gnss_svs_ui, gnss_svs_ui);

const ntpd_status_lookup_string = {
  0: "Unsynchronized",
  1: "In Lock",
  2: "In Holdover",
  3: "Degraded (Holdover expired)"
}

var ntpd_ui = {
  view: function()
  {
    return m("div", [
      m("p", m("b", "Status: "), `${ntpd_status_lookup_string[status_ntpd.status] || "??"}`),
      m("p", m("b", "Reported Stratum: "), `${status_ntpd.stratum}`),
      m("p", m("b", "Requests: "), `${status_ntpd.requests_count}`),
    ]);
  }
}

m.mount(doc_ntpd_ui, ntpd_ui);

function status_update()
{
  m.request({
    method: "GET",
    url: "/api/status"
  })
  .then(function(result)
  {
    //console.log(result);
    status_gnss = result.gnss;
    status_svs = result.svs;
    status_ntpd = result.ntpd;

    drawSVsPolar();
  });
}

status_update();
setInterval(status_update, 1000);


/* SVs Polar Projection derived from https://github.com/berthubert/galmon */
/* (C) AHU Holding BV - bert@hubertnet.nl - https://berthub.eu/ */

function flippedStereographic(x, y)  {
    var cx = Math.cos(x), cy = Math.cos(y), k = 1 / (1 + cx * cy);
    return [k * cy * Math.sin(x), -k * Math.sin(y)];
}

function drawSVsPolar()
{
  var sats = d3.select("#Orbits-Drawing").html("").append("svg");

  const width = 350; //sats.clientWidth;
  const height = 350; //sats.clientHeight;

  sats.attr("width", width);
  sats.attr("height", height);
  
  var projection = d3.geoProjection(flippedStereographic)
    .scale(width * 0.40)
    .clipAngle(130)
    .rotate([0, -90])
    .translate([width / 2 + 0.5, height / 2 + 0.5])
    .precision(1);

  var path = d3.geoPath().projection(projection);

  sats.append("path")
    .datum(d3.geoCircle().center([0, 90]).radius(90))
    .attr("stroke-width", 1.5)
    .attr("d", path);

  sats.append("path")
    .datum(d3.geoGraticule())
    .attr("stroke-width", 0.15)
    .attr("d", path);
//    .attr("fill", "none").attr("stroke", "black").attr("width", "100%").attr("height", "100%");


  sats.append("g")
    .selectAll("line")
    .data(d3.range(360))
    .enter().append("line")
    .each(function(d) {
      var p0 = projection([d, 0]),
          p1 = projection([d, d % 10 ? -1 : -2]);
      
      d3.select(this)
        .attr("x1", p0[0])
        .attr("y1", p0[1])
        .attr("x2", p1[0])
        .attr("y2", p1[1]);
    });
  
  /* Azimuth Labels */
  sats.append("g")
    .attr("fill", "black")
    .attr("stroke", "none")
    .selectAll("text")
    .data(d3.range(0, 360, 90))
    .enter().append("text")
    .each(function(d) {
      var p = projection([d, -4]);
      d3.select(this).attr("x", p[0]).attr("y", p[1]);
    })
    .attr("dy", "0.35em")
    .text(function(d) { return d === 0 ? "N" : d === 90 ? "E" : d === 180 ? "S" : d === 270 ? "W" : d + "°"; })
    .attr("font-weight", "bold")
    .attr("font-size", 14);
  
  /* Elevation Labels */
  sats.append("g")
    .attr("fill", "#A3ACA9")
    .attr("stroke", "none")
    .selectAll("text")
    .data(d3.range(10, 91, 10))
    .enter().append("text")
    .each(function(d) {
      var p = projection([0, d]);
      d3.select(this).attr("x", p[0]).attr("y", p[1]);
    })
    .attr("dy", "-0.4em")
    .text(function(d) { return d + "°"; });

  sats.select('g.satellites').remove();
  
  let points = sats
    .insert("g")
    .attr("class", "satellites")
    .selectAll('g.satellite')
    .data(status_svs)
    .enter()
    .append('g')
    .attr("transform", function(d) {
      var p = projection([d.az, d.el]);
      return 'translate(' + p[0] + ', ' + p[1] + ')';
    });
          
  points
    .attr('class', 'satellite')
    .append("circle")
    .attr("stroke", function(d) {
      return d.cn0 > 0 ? "transparent" : d[5];
    })
    .attr("r", function(d) {
      return d.cn0 > 0 ? d.cn0*0.125 : 3;
    })
    .attr("fill", function(d) {
      return d.cn0 > 0 ? (gnss_lookup_colour[d.gnss] || "transparent") : "transparent";
    });

   points
    .attr("r", 50)
    .append("text")
    .attr("class", "sats-label")
    .attr('dy', function(d) {
        return d.cn0 > 0 ? `${10+(d.cn0/8)}px` : "1.3em";
    })
    .attr('dx', function(d) {
        return d.cn0 > 0 ? `${3+(d.cn0/8)}px` : "0.7em";
    })
    .text(function(d){return `${gnss_lookup_letter[d.gnss] || "?"}${d.sv}`});
}