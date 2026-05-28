/**
 * @file
 * D3 v5 analytics charts for Dynamic Form Analytics module.
 *
 * Reads data from Drupal.settings.dfAnalytics and renders charts
 * into the mount elements placed by the PHP page callbacks.
 *
 * Chart types:
 *   renderLineChart  — time-series submissions trend (line + area)
 *   renderHBarChart  — horizontal bar for option/tag distributions
 *   renderVBarChart  — vertical bar for date month distributions
 *   renderDonutChart — donut for status/respondent/file-type breakdowns
 */

(function ($) {

  'use strict';

  var COLORS = ['#4f6ef7', '#22c55e', '#f59e0b', '#ef4444', '#8b5cf6', '#06b6d4', '#f97316', '#ec4899'];

  function _esc(str) {
    return $('<div>').text(String(str || '')).html();
  }

  /* ================================================================
     TOOLTIP helper — single shared DOM element
     ================================================================ */

  var tooltip     = null;
  var _hideTimer  = null;
  var _activeTag  = null;
  var _tagFormsCache = {};

  function getTooltip() {
    if (!tooltip) {
      tooltip = d3.select('body')
        .append('div')
        .attr('class', 'dfa-tooltip')
        .on('mouseenter', cancelHideTooltip)
        .on('mouseleave', hideTooltip);
    }
    return tooltip;
  }

  function showTooltip(html, event) {
    cancelHideTooltip();
    getTooltip()
      .html(html)
      .style('opacity', 1)
      .style('left', (event.pageX + 12) + 'px')
      .style('top',  (event.pageY - 28) + 'px');
  }

  function hideTooltip() {
    cancelHideTooltip();
    _activeTag = null;
    if (tooltip) {
      tooltip
        .classed('dfa-tooltip--interactive', false)
        .style('opacity', 0);
    }
  }

  function scheduleHideTooltip() {
    cancelHideTooltip();
    _hideTimer = setTimeout(hideTooltip, 250);
  }

  function cancelHideTooltip() {
    if (_hideTimer) { clearTimeout(_hideTimer); _hideTimer = null; }
  }

  /* ================================================================
     WORD CLOUD — per-form tag drilldown tooltip
     ================================================================ */

  function _wcShowTagForms(ajaxTagForms, tag) {
    if (_tagFormsCache.hasOwnProperty(tag)) {
      _wcRenderTagForms(_tagFormsCache[tag], tag);
      return;
    }
    d3.json(ajaxTagForms + '?tag=' + encodeURIComponent(tag))
      .then(function (data) {
        _tagFormsCache[tag] = data.forms || [];
        if (_activeTag === tag) {
          _wcRenderTagForms(_tagFormsCache[tag], tag);
        }
      })
      .catch(function () {
        if (_activeTag === tag) {
          _wcRenderTagForms([], tag);
        }
      });
  }

  function _wcRenderTagForms(forms, tag) {
    if (_activeTag !== tag || !tooltip) { return; }
    var base = (Drupal.settings && Drupal.settings.basePath) ? Drupal.settings.basePath : '/';
    var html = '<strong>' + _esc(tag) + '</strong>';
    if (!forms || !forms.length) {
      html += '<span class="dfa-wc-no-forms"> &mdash; no data</span>';
      getTooltip().html(html).style('opacity', 1);
      return;
    }
    html += '<ul class="dfa-wc-forms-list">';
    forms.forEach(function (f) {
      var href = base + 'dashboard/forms/' + parseInt(f.form_id, 10) + '/responses'
        + '?f%5B0%5D%5Bqid%5D=' + parseInt(f.question_id, 10)
        + '&f%5B0%5D%5Bval%5D=' + encodeURIComponent(tag);
      html += '<li class="dfa-wc-form-item">'
        + '<a href="' + href + '" class="dfa-wc-form-link">' + _esc(f.title) + '</a>'
        + '<span class="dfa-wc-form-count">' + _esc(f.count) + '</span>'
        + '</li>';
    });
    html += '</ul>';
    getTooltip()
      .classed('dfa-tooltip--interactive', true)
      .html(html)
      .style('opacity', 1);
  }

  /* ================================================================
     LINE CHART — time-series (submissions over time)
     el: DOM element, data: [{day:'YYYY-MM-DD', cnt:N}, ...]
     ================================================================ */

  function renderLineChart(el, data) {
    var margin = { top: 16, right: 20, bottom: 36, left: 44 };
    var totalW  = el.clientWidth || 600;
    var totalH  = 220;
    var W = totalW - margin.left - margin.right;
    var H = totalH - margin.top  - margin.bottom;

    d3.select(el).selectAll('*').remove();

    var parseDay = d3.timeParse('%Y-%m-%d');
    var points   = data.map(function (d) {
      return { day: parseDay(d.day), cnt: +d.cnt };
    });

    var x = d3.scaleTime()
      .domain(d3.extent(points, function (d) { return d.day; }))
      .range([0, W]);

    var y = d3.scaleLinear()
      .domain([0, d3.max(points, function (d) { return d.cnt; }) * 1.15])
      .nice()
      .range([H, 0]);

    var svg = d3.select(el)
      .append('svg')
      .attr('width',  totalW)
      .attr('height', totalH)
      .append('g')
      .attr('transform', 'translate(' + margin.left + ',' + margin.top + ')');

    // Dashed grid lines.
    svg.append('g')
      .attr('class', 'dfa-grid')
      .call(d3.axisLeft(y).ticks(4).tickSize(-W).tickFormat(''));

    // Gradient fill under line.
    var gradId = 'dfa-grad-' + Math.random().toString(36).slice(2, 7);
    var defs = svg.append('defs');
    var grad = defs.append('linearGradient')
      .attr('id', gradId)
      .attr('x1', '0').attr('x2', '0')
      .attr('y1', '0').attr('y2', '1');
    grad.append('stop').attr('offset', '0%').attr('stop-color', COLORS[0]).attr('stop-opacity', 0.25);
    grad.append('stop').attr('offset', '100%').attr('stop-color', COLORS[0]).attr('stop-opacity', 0.02);

    var area = d3.area()
      .x(function (d) { return x(d.day); })
      .y0(H)
      .y1(function (d) { return y(d.cnt); })
      .curve(d3.curveCatmullRom.alpha(0.5));

    svg.append('path')
      .datum(points)
      .attr('fill', 'url(#' + gradId + ')')
      .attr('d', area);

    var line = d3.line()
      .x(function (d) { return x(d.day); })
      .y(function (d) { return y(d.cnt); })
      .curve(d3.curveCatmullRom.alpha(0.5));

    svg.append('path')
      .datum(points)
      .attr('fill', 'none')
      .attr('stroke', COLORS[0])
      .attr('stroke-width', 2.5)
      .attr('d', line);

    // Dot per data point.
    svg.selectAll('.dfa-dot')
      .data(points)
      .enter()
      .append('circle')
      .attr('class', 'dfa-dot')
      .attr('cx', function (d) { return x(d.day); })
      .attr('cy', function (d) { return y(d.cnt); })
      .attr('r', 4)
      .attr('fill', '#fff')
      .attr('stroke', COLORS[0])
      .attr('stroke-width', 2)
      .on('mouseover', function (d) {
        showTooltip(d3.timeFormat('%b %d')(d.day) + ': <strong>' + d.cnt + '</strong>', d3.event);
      })
      .on('mouseout', hideTooltip);

    // Axes.
    svg.append('g')
      .attr('class', 'dfa-axis')
      .attr('transform', 'translate(0,' + H + ')')
      .call(d3.axisBottom(x).ticks(6).tickFormat(d3.timeFormat('%b %d')));

    svg.append('g')
      .attr('class', 'dfa-axis')
      .call(d3.axisLeft(y).ticks(4));
  }

  /* ================================================================
     HORIZONTAL BAR CHART — option/tag distributions
     el: DOM element, data: [{label:'...', cnt:N}, ...]
     ================================================================ */

  function renderHBarChart(el, data) {
    var BAR_H   = 28;
    var BAR_GAP = 8;
    var margin  = { top: 8, right: 56, bottom: 8, left: 160 };
    var totalW  = el.clientWidth || 600;
    var totalH  = margin.top + (data.length * (BAR_H + BAR_GAP)) + margin.bottom;
    var W = totalW - margin.left - margin.right;

    // Clamp label column to available space.
    if (margin.left > totalW * 0.4) {
      margin.left = Math.floor(totalW * 0.4);
      W = totalW - margin.left - margin.right;
    }

    d3.select(el).selectAll('*').remove();

    el.style.height = totalH + 'px';

    var maxCnt = d3.max(data, function (d) { return d.cnt; }) || 1;

    var x = d3.scaleLinear().domain([0, maxCnt]).range([0, W]);
    var y = d3.scaleBand()
      .domain(data.map(function (d) { return d.label; }))
      .range([0, data.length * (BAR_H + BAR_GAP)])
      .padding(0.15);

    var svg = d3.select(el)
      .append('svg')
      .attr('width',  totalW)
      .attr('height', totalH)
      .append('g')
      .attr('transform', 'translate(' + margin.left + ',' + margin.top + ')');

    // Bars.
    svg.selectAll('.dfa-bar')
      .data(data)
      .enter()
      .append('rect')
      .attr('class', 'dfa-bar')
      .attr('y',      function (d) { return y(d.label); })
      .attr('height', y.bandwidth())
      .attr('x', 0)
      .attr('width',  function (d) { return x(d.cnt); })
      .attr('fill',   function (d, i) { return COLORS[i % COLORS.length]; })
      .attr('rx', 4)
      .on('mouseover', function (d) {
        showTooltip('<strong>' + d.label + '</strong>: ' + d.cnt, d3.event);
      })
      .on('mouseout', hideTooltip);

    // Value labels on bars.
    svg.selectAll('.dfa-bar-label')
      .data(data)
      .enter()
      .append('text')
      .attr('class', 'dfa-bar-label')
      .attr('x',  function (d) { return x(d.cnt) + 6; })
      .attr('y',  function (d) { return y(d.label) + y.bandwidth() / 2; })
      .attr('dy', '0.35em')
      .attr('font-size', 11)
      .attr('fill', '#64748b')
      .text(function (d) { return d.cnt; });

    // Y-axis (labels on left).
    var yAxis = d3.axisLeft(y).tickSize(0).tickPadding(8);
    svg.append('g')
      .attr('class', 'dfa-axis')
      .call(yAxis)
      .select('.domain').remove();

    // Truncate long labels (guard against axis elements with no bound datum).
    svg.selectAll('.dfa-axis text')
      .each(function (d) {
        if (typeof d !== 'string') { return; }
        var self  = d3.select(this);
        var label = d.length > 22 ? d.slice(0, 21) + '…' : d;
        self.text(label);
      });
  }

  /* ================================================================
     VERTICAL BAR CHART — date (month) distributions
     el: DOM element, data: [{label:'YYYY-MM', cnt:N}, ...]
     ================================================================ */

  function renderVBarChart(el, data) {
    var margin = { top: 16, right: 16, bottom: 46, left: 44 };
    var totalW  = el.clientWidth || 600;
    var totalH  = 200;
    var W = totalW - margin.left - margin.right;
    var H = totalH - margin.top  - margin.bottom;

    d3.select(el).selectAll('*').remove();

    var maxCnt = d3.max(data, function (d) { return d.cnt; }) || 1;

    var x = d3.scaleBand()
      .domain(data.map(function (d) { return d.label; }))
      .range([0, W])
      .padding(0.25);

    var y = d3.scaleLinear()
      .domain([0, maxCnt * 1.15])
      .nice()
      .range([H, 0]);

    var svg = d3.select(el)
      .append('svg')
      .attr('width',  totalW)
      .attr('height', totalH)
      .append('g')
      .attr('transform', 'translate(' + margin.left + ',' + margin.top + ')');

    svg.append('g')
      .attr('class', 'dfa-grid')
      .call(d3.axisLeft(y).ticks(4).tickSize(-W).tickFormat(''));

    svg.selectAll('.dfa-vbar')
      .data(data)
      .enter()
      .append('rect')
      .attr('class', 'dfa-vbar')
      .attr('x',      function (d) { return x(d.label); })
      .attr('y',      function (d) { return y(d.cnt); })
      .attr('width',  x.bandwidth())
      .attr('height', function (d) { return H - y(d.cnt); })
      .attr('fill',   COLORS[0])
      .attr('rx', 3)
      .on('mouseover', function (d) {
        showTooltip(d.label + ': <strong>' + d.cnt + '</strong>', d3.event);
      })
      .on('mouseout', hideTooltip);

    // X axis — show every Nth label to avoid overlap.
    var tickEvery = Math.max(1, Math.ceil(data.length / Math.floor(W / 60)));
    svg.append('g')
      .attr('class', 'dfa-axis')
      .attr('transform', 'translate(0,' + H + ')')
      .call(d3.axisBottom(x).tickValues(
        data.filter(function (d, i) { return i % tickEvery === 0; }).map(function (d) { return d.label; })
      ).tickFormat(function (d) {
        var parts = d.split('-');
        var months = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
        return months[parseInt(parts[1], 10) - 1] + ' ' + parts[0].slice(2);
      }))
      .selectAll('text')
      .attr('transform', 'rotate(-35)')
      .attr('text-anchor', 'end')
      .attr('dx', '-4')
      .attr('dy', '6');

    svg.append('g')
      .attr('class', 'dfa-axis')
      .call(d3.axisLeft(y).ticks(4));
  }

  /* ================================================================
     DONUT CHART — status / respondent type / file type breakdown
     el: DOM element, data: [{label:'...', cnt:N}, ...]
     centerLabel: optional string displayed in the hole
     ================================================================ */

  function renderDonutChart(el, data, centerLabel) {
    var totalW  = el.clientWidth || 320;
    var totalH  = 200;
    var radius  = Math.min(totalW, totalH) / 2 - 10;
    var inner   = radius * 0.55;

    d3.select(el).selectAll('*').remove();

    var svg = d3.select(el)
      .append('svg')
      .attr('width',  totalW)
      .attr('height', totalH)
      .append('g')
      .attr('transform', 'translate(' + (totalW / 2) + ',' + (totalH / 2) + ')');

    var pie  = d3.pie().sort(null).value(function (d) { return d.cnt; });
    var arc  = d3.arc().innerRadius(inner).outerRadius(radius);
    var hArc = d3.arc().innerRadius(inner).outerRadius(radius + 6);

    var arcs = svg.selectAll('.dfa-arc')
      .data(pie(data))
      .enter()
      .append('g')
      .attr('class', 'dfa-arc');

    arcs.append('path')
      .attr('d', arc)
      .attr('fill', function (d, i) { return COLORS[i % COLORS.length]; })
      .on('mouseover', function (d) {
        d3.select(this).attr('d', hArc);
        showTooltip('<strong>' + d.data.label + '</strong>: ' + d.data.cnt, d3.event);
      })
      .on('mouseout', function (d) {
        d3.select(this).attr('d', arc);
        hideTooltip();
      });

    // Center text.
    var total = d3.sum(data, function (d) { return d.cnt; });
    svg.append('text')
      .attr('text-anchor', 'middle')
      .attr('dy', '-0.2em')
      .attr('font-size', 20)
      .attr('font-weight', 700)
      .attr('fill', '#1e293b')
      .text(centerLabel !== undefined ? centerLabel : total);

    svg.append('text')
      .attr('text-anchor', 'middle')
      .attr('dy', '1.2em')
      .attr('font-size', 11)
      .attr('fill', '#94a3b8')
      .text('total');

    // Small legend below donut.
    var legendG = svg.append('g')
      .attr('transform', 'translate(' + (-totalW / 2 + 8) + ',' + (radius + 18) + ')');

    data.forEach(function (d, i) {
      var row = legendG.append('g')
        .attr('transform', 'translate(' + (i * (totalW / data.length)) + ',0)');
      row.append('rect')
        .attr('width', 10).attr('height', 10).attr('rx', 2)
        .attr('fill', COLORS[i % COLORS.length]);
      row.append('text')
        .attr('x', 14).attr('y', 9)
        .attr('font-size', 10).attr('fill', '#64748b')
        .text(d.label);
    });
  }

  /* ================================================================
     RADAR CHART — 3-segment (visibility) × 4-ring (submission vol)
     el:    DOM element (height must be set via CSS before calling)
     forms: [{id, title, visibility, total_started, total_submitted,
              completion_rate}, ...]
     ================================================================ */

  function renderRadarChart(el, forms) {
    var W      = el.clientWidth  || 700;
    var H      = el.clientHeight || 520;
    var cx     = W / 2;
    var cy     = H / 2 + 10;
    var outerR = Math.min(W * 0.38, (H - 130) / 2);
    var innerR = outerR * 0.12;

    d3.select(el).selectAll('*').remove();

    if (!forms || !forms.length) {
      d3.select(el)
        .append('div')
        .attr('class', 'dfb-analytics-empty')
        .text('No form data available.');
      return;
    }

    var segments = [
      { key: 'p', label: 'Public',       color: '#4f6ef7' },
      { key: 'r', label: 'Restricted',   color: '#f59e0b' },
      { key: 'm', label: 'Members Only', color: '#8b5cf6' },
    ];
    var numSeg   = segments.length;
    var segAngle = (2 * Math.PI) / numSeg;

    var ringBands = [
      { label: 'Emerging', min: 0,   max: 5   },
      { label: 'Growing',  min: 6,   max: 25  },
      { label: 'Active',   min: 26,  max: 100 },
      { label: 'Popular',  min: 101, max: Infinity },
    ];
    var numRings = ringBands.length;

    function getRadius(submitted) {
      for (var i = 0; i < ringBands.length; i++) {
        var b = ringBands[i];
        if (submitted <= b.max || i === numRings - 1) {
          var rStart = innerR + (outerR - innerR) * i / numRings;
          var rEnd   = innerR + (outerR - innerR) * (i + 1) / numRings;
          var t = (b.max === Infinity || b.max === b.min)
            ? 0.5
            : Math.min((submitted - b.min) / (b.max - b.min), 1);
          return rStart + t * (rEnd - rStart);
        }
      }
      return outerR;
    }

    var colorScale = d3.scaleSequential(d3.interpolateRdYlGn).domain([0, 1]);

    var svg = d3.select(el).append('svg').attr('width', W).attr('height', H);
    var g   = svg.append('g').attr('transform', 'translate(' + cx + ',' + cy + ')');

    // Alternating ring fills, outermost first.
    for (var ri = numRings; ri >= 1; ri--) {
      var rr = innerR + (outerR - innerR) * ri / numRings;
      g.append('circle')
        .attr('r', rr)
        .attr('fill',         ri % 2 === 0 ? '#f8fafc' : '#f1f5f9')
        .attr('stroke',       '#e2e8f0')
        .attr('stroke-width', 1);
    }

    // Segment arc fills + divider borders.
    segments.forEach(function (seg, si) {
      var a0  = si * segAngle;
      var a1  = a0 + segAngle;
      var arc = d3.arc()
        .innerRadius(innerR).outerRadius(outerR)
        .startAngle(a0).endAngle(a1);

      g.append('path')
        .attr('d',            arc)
        .attr('fill',         seg.color)
        .attr('fill-opacity', 0.05)
        .attr('stroke',       '#cbd5e1')
        .attr('stroke-width', 1);

      var midA   = a0 + segAngle / 2;
      var labelR = outerR + 30;
      g.append('text')
        .attr('x',           labelR * Math.sin(midA))
        .attr('y',           -labelR * Math.cos(midA))
        .attr('text-anchor', 'middle')
        .attr('dy',          '0.35em')
        .attr('font-size',   12)
        .attr('font-weight', 600)
        .attr('fill',        seg.color)
        .text(seg.label);
    });

    // Ring labels near the top of each ring.
    ringBands.forEach(function (band, ri) {
      var r = innerR + (outerR - innerR) * (ri + 1) / numRings;
      g.append('text')
        .attr('x',         5)
        .attr('y',         -r - 3)
        .attr('font-size', 9)
        .attr('fill',      '#94a3b8')
        .text(band.label);
    });

    // Center hub.
    g.append('circle')
      .attr('r',            innerR)
      .attr('fill',         '#fff')
      .attr('stroke',       '#e2e8f0')
      .attr('stroke-width', 1);

    // Group forms by segment.
    var segMap = { p: [], r: [], m: [] };
    forms.forEach(function (f) {
      var key = segMap[f.visibility] ? f.visibility : 'p';
      segMap[key].push(f);
    });

    // Plot dots.
    segments.forEach(function (seg, si) {
      var segForms = segMap[seg.key];
      var a0       = si * segAngle;
      var margin   = 0.18;
      var usable   = segAngle - margin * 2;

      segForms.forEach(function (f, fi) {
        var t      = segForms.length === 1 ? 0.5 : fi / (segForms.length - 1);
        var angle  = a0 + margin + t * usable;
        var radius = f.total_submitted === 0 ? innerR * 0.6 : getRadius(f.total_submitted);
        var x      = radius * Math.sin(angle);
        var y      = -radius * Math.cos(angle);

        g.append('circle')
          .datum(f)
          .attr('cx',           x)
          .attr('cy',           y)
          .attr('r',            6)
          .attr('fill',         colorScale(f.completion_rate || 0))
          .attr('stroke',       '#fff')
          .attr('stroke-width', 1.5)
          .attr('cursor',       'pointer')
          .on('mouseover', function (d) {
            d3.select(this).attr('r', 9);
            showTooltip(
              '<strong>' + d.title + '</strong><br>' +
              'Submitted: ' + d.total_submitted + ' / ' + d.total_started + '<br>' +
              'Completion: ' + Math.round((d.completion_rate || 0) * 100) + '%',
              d3.event
            );
          })
          .on('mouseout', function () {
            d3.select(this).attr('r', 6);
            hideTooltip();
          })
          .on('click', function (d) {
            window.location.href = '/dynamic-form-builder/dashboard/forms/' + d.id + '/responses';
          });
      });
    });

  }

  /* ================================================================
     WORD CLOUD — d3-cloud (Jason Davies) layout
     el: DOM element
     words: [{text:'...', count:N}, ...] sorted by count DESC
     ================================================================ */

  function renderWordCloud(el, words, options) {
    options = options || {};
    var ajaxTagForms  = options.ajaxTagForms  || null;
    var responsesBase = options.responsesBase || null;

    var W = el.clientWidth || 700;
    var H = 300;
    var pad = 30;

    d3.select(el).selectAll('*').remove();

    if (!words || !words.length) {
      d3.select(el)
        .append('div')
        .attr('class', 'dfb-analytics-empty')
        .text('No tag data available for the selected filter.');
      return;
    }

    var maxCount = d3.max(words, function (d) { return d.count; }) || 1;
    var minCount = d3.min(words, function (d) { return d.count; }) || 1;

    var fontScale = (maxCount === minCount)
      ? function () { return 24; }
      : d3.scaleSqrt().domain([minCount, maxCount]).range([12, 48]).clamp(true);

    var wordData = words.map(function (d, i) {
      return {
        text:        d.text,
        count:       d.count,
        question_id: d.question_id || 0,
        size:        fontScale(d.count),
        color:       COLORS[i % COLORS.length]
      };
    });

    d3.layout.cloud()
      .size([W - pad * 2, H - pad * 2])
      .words(wordData)
      .padding(5)
      .rotate(0)
      .font('sans-serif')
      .fontSize(function (d) { return d.size; })
      .on('end', function (placed) {
        var svg = d3.select(el)
          .append('svg')
          .attr('width',  W)
          .attr('height', H)
          .append('g')
          .attr('transform', 'translate(' + (W / 2) + ',' + (H / 2) + ')');

        svg.selectAll('text')
          .data(placed)
          .enter()
          .append('text')
          .attr('text-anchor', 'middle')
          .attr('transform', function (d) {
            return 'translate(' + [d.x, d.y] + ')rotate(' + d.rotate + ')';
          })
          .attr('font-size', function (d) { return d.size + 'px'; })
          .attr('fill',      function (d) { return d.color; })
          .style('cursor', (ajaxTagForms || responsesBase) ? 'pointer' : 'default')
          .text(function (d) { return d.text; })
          .on('mouseover', function (d) {
            var ev = d3.event;
            cancelHideTooltip();
            _activeTag = d.text;

            if (ajaxTagForms) {
              showTooltip(
                '<strong>' + d.text + '</strong>: ' + d.count + (d.count === 1 ? ' use' : ' uses')
                + '<div class="dfa-wc-loading-mini">Loading…</div>',
                ev
              );
              _wcShowTagForms(ajaxTagForms, d.text);
            } else if (responsesBase) {
              showTooltip(
                '<strong>' + d.text + '</strong>: ' + d.count + (d.count === 1 ? ' use' : ' uses')
                + '<div class="dfa-wc-click-hint">Click to filter responses</div>',
                ev
              );
            } else {
              showTooltip(
                '<strong>' + d.text + '</strong>: ' + d.count + (d.count === 1 ? ' use' : ' uses'),
                ev
              );
            }
          })
          .on('mouseout', function () { scheduleHideTooltip(); })
          .on('click', function (d) {
            if (responsesBase) {
              var qid = d.question_id || 0;
              var url = responsesBase
                + '?f%5B0%5D%5Bqid%5D=' + qid
                + '&f%5B0%5D%5Bval%5D=' + encodeURIComponent(d.text);
              window.location.href = url;
            }
          });
      })
      .start();
  }

  /* ================================================================
     WORD CLOUD AJAX helpers
     ================================================================ */

  function _wcSetLoading(el, loading) {
    d3.select(el).selectAll('*').remove();
    if (loading) {
      d3.select(el)
        .append('div')
        .attr('class', 'dfb-wc-loading')
        .text('Loading…');
    }
  }

  function wcFetch(ajaxUrl, formId, questionId, mountEl, options) {
    _wcSetLoading(mountEl, true);
    var url = ajaxUrl + '?form_id=' + (formId || 0) + '&question_id=' + (questionId || 0);
    d3.json(url)
      .then(function (data) {
        renderWordCloud(mountEl, data.words, options);
      })
      .catch(function () {
        d3.select(mountEl).selectAll('*').remove();
        d3.select(mountEl)
          .append('div')
          .attr('class', 'dfb-analytics-empty')
          .text('Could not load word cloud data.');
      });
  }

  /* ================================================================
     DRUPAL BEHAVIOR — wires chart functions to DOM elements
     ================================================================ */

  Drupal.behaviors.dfAnalytics = {
    attach: function (context, settings) {
      if (!settings.dfAnalytics) { return; }

      /* ---------- RADAR PAGE ---------- */
      if (settings.dfAnalytics.radar) {
        var radarEl = document.getElementById('dfa-chart-radar');
        if (radarEl) {
          renderRadarChart(radarEl, settings.dfAnalytics.radar);
        }
      }

      /* ---------- GLOBAL WORD CLOUD ---------- */
      if (settings.dfAnalytics.wordcloud) {
        var wc      = settings.dfAnalytics.wordcloud;
        var wcEl    = document.getElementById('dfa-chart-wordcloud');
        var wcForm  = document.getElementById('dfa-wc-form');
        var wcQ     = document.getElementById('dfa-wc-question');
        var wcOpts  = { ajaxTagForms: wc.ajaxTagForms };

        if (wcEl) {
          // Populate form dropdown.
          if (wcForm && wc.forms && wc.forms.length) {
            wc.forms.forEach(function (f) {
              var opt = document.createElement('option');
              opt.value       = f.id;
              opt.textContent = f.title;
              wcForm.appendChild(opt);
            });
          }

          // Render initial cloud (all forms).
          renderWordCloud(wcEl, wc.words, wcOpts);

          // Form filter change.
          if (wcForm) {
            $(wcForm).on('change', function () {
              var fid = parseInt(this.value, 10) || 0;

              // Reset and disable question dropdown.
              $(wcQ).empty().append('<option value="">All Tag Questions</option>').prop('disabled', true);

              if (!fid) {
                // Back to global view.
                renderWordCloud(wcEl, wc.words, wcOpts);
                return;
              }

              // Load tag questions for this form via AJAX.
              $.getJSON(wc.ajaxQuestions + '/' + fid, function (resp) {
                if (resp && resp.questions && resp.questions.length) {
                  resp.questions.forEach(function (q) {
                    var opt = document.createElement('option');
                    opt.value       = q.id;
                    opt.textContent = q.label;
                    wcQ.appendChild(opt);
                  });
                  $(wcQ).prop('disabled', false);
                }
              });

              // Fetch word cloud for this form.
              wcFetch(wc.ajaxData, fid, 0, wcEl, wcOpts);
            });
          }

          // Question filter change.
          if (wcQ) {
            $(wcQ).on('change', function () {
              var fid = parseInt($(wcForm).val(), 10) || 0;
              var qid = parseInt(this.value, 10) || 0;
              wcFetch(wc.ajaxData, fid, qid, wcEl, wcOpts);
            });
          }
        }
      }

      /* ---------- GLOBAL PAGE ---------- */
      if (settings.dfAnalytics.global) {
        var g = settings.dfAnalytics.global;

        // Submissions trend (line).
        var trendEl = document.getElementById('dfa-chart-global-trend');
        if (trendEl && g.trend && g.trend.length) {
          renderLineChart(trendEl, g.trend);
        }

        // Status donut.
        var statusEl = document.getElementById('dfa-chart-status-donut');
        if (statusEl && g.status_data) {
          renderDonutChart(statusEl, g.status_data);
        }

        // Top forms horizontal bar.
        var topEl = document.getElementById('dfa-chart-top-forms');
        if (topEl && g.top_forms && g.top_forms.length) {
          renderHBarChart(topEl, g.top_forms);
        }
      }

      /* ---------- PER-FORM WORD CLOUD ---------- */
      if (settings.dfAnalytics.wordcloudForm) {
        var wcf    = settings.dfAnalytics.wordcloudForm;
        var wcfEl  = document.getElementById('dfa-chart-wordcloud-form');
        var wcfQ   = document.getElementById('dfa-wc-question-form');
        var wcfOpts = { responsesBase: wcf.responsesBase };

        if (wcfEl) {
          // Populate question dropdown.
          if (wcfQ && wcf.questions && wcf.questions.length) {
            wcf.questions.forEach(function (q) {
              var opt = document.createElement('option');
              opt.value       = q.id;
              opt.textContent = q.label;
              wcfQ.appendChild(opt);
            });
          }

          // Render initial cloud for this form.
          renderWordCloud(wcfEl, wcf.words, wcfOpts);

          // Question filter change.
          if (wcfQ) {
            $(wcfQ).on('change', function () {
              var qid = parseInt(this.value, 10) || 0;
              wcFetch(wcf.ajaxData, wcf.formId, qid, wcfEl, wcfOpts);
            });
          }
        }
      }

      /* ---------- PER-FORM PAGE ---------- */
      if (settings.dfAnalytics.form) {
        var f = settings.dfAnalytics.form;

        // Daily trend (line).
        var formTrendEl = document.getElementById('dfa-chart-form-trend');
        if (formTrendEl && f.trend && f.trend.length) {
          renderLineChart(formTrendEl, f.trend);
        }

        // Completion funnel (donut: submitted vs abandoned).
        var funnelEl = document.getElementById('dfa-chart-funnel');
        if (funnelEl && f.kpi) {
          renderDonutChart(funnelEl, [
            { label: 'Submitted', cnt: f.kpi.total_submitted },
            { label: 'Abandoned', cnt: f.kpi.abandoned }
          ], f.kpi.total_started);
        }

        // Respondent type donut.
        var respEl = document.getElementById('dfa-chart-respondent');
        if (respEl && f.respondent) {
          renderDonutChart(respEl, f.respondent);
        }

        // Per-question charts.
        if (f.questions) {
          f.questions.forEach(function (q) {
            var qEl = document.getElementById('dfa-q-' + q.id);
            if (!qEl || !q.data || !q.data.length) { return; }

            switch (q.chart) {
              case 'hbar':
                renderHBarChart(qEl, q.data);
                break;
              case 'vbar':
                renderVBarChart(qEl, q.data);
                break;
              case 'donut':
                renderDonutChart(qEl, q.data, q.total_files !== undefined ? q.total_files : undefined);
                break;
            }
          });
        }
      }
    }
  };

}(jQuery));
