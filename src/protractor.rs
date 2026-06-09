use serde::{Serialize, Deserialize};

#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct Point2D {
    pub x: f64,
    pub y: f64,
}

pub fn path_length(points: &[Point2D]) -> f64 {
    let mut len = 0.0;
    for i in 1..points.len() {
        let dx = points[i].x - points[i - 1].x;
        let dy = points[i].y - points[i - 1].y;
        len += (dx * dx + dy * dy).sqrt();
    }
    len
}

pub fn resample_path(points: &[Point2D], n: usize) -> Vec<Point2D> {
    if n == 0 {
        return Vec::new();
    }
    if points.is_empty() {
        return vec![Point2D { x: 0.0, y: 0.0 }; n];
    }
    if points.len() == 1 {
        return vec![points[0]; n];
    }

    let len = path_length(points);
    let interval = if len > 1e-4 { len / (n - 1) as f64 } else { 0.0 };
    let mut resampled = Vec::with_capacity(n);
    resampled.push(points[0]);

    let mut d_accum = 0.0;
    let mut pts = points.to_vec();

    let mut i = 1;
    while i < pts.len() && resampled.len() < n {
        let dx = pts[i].x - pts[i - 1].x;
        let dy = pts[i].y - pts[i - 1].y;
        let d = (dx * dx + dy * dy).sqrt();
        if (d_accum + d) >= interval && interval > 1e-4 {
            let qx = pts[i - 1].x + ((interval - d_accum) / d) * dx;
            let qy = pts[i - 1].y + ((interval - d_accum) / d) * dy;
            let q = Point2D { x: qx, y: qy };
            resampled.push(q);
            pts[i - 1] = q;
            d_accum = 0.0;
            // check the same segment again (don't increment i)
        } else {
            d_accum += d;
            i += 1;
        }
    }

    while resampled.len() < n {
        resampled.push(points[points.len() - 1]);
    }

    resampled
}

pub fn vectorize_path(points: &mut [Point2D]) {
    if points.is_empty() {
        return;
    }
    let count = points.len() as f64;
    let mut sum_x = 0.0;
    let mut sum_y = 0.0;
    for p in points.iter() {
        sum_x += p.x;
        sum_y += p.y;
    }
    let centroid_x = sum_x / count;
    let centroid_y = sum_y / count;
    for p in points.iter_mut() {
        p.x -= centroid_x;
        p.y -= centroid_y;
    }

    let mut sum_squares = 0.0;
    for p in points.iter() {
        sum_squares += p.x * p.x + p.y * p.y;
    }
    let mut magnitude = sum_squares.sqrt();
    if magnitude < 1e-6 {
        magnitude = 1e-6;
    }
    for p in points.iter_mut() {
        p.x /= magnitude;
        p.y /= magnitude;
    }
}

pub fn compute_protractor_similarity(u: &[Point2D], w: &[Point2D], max_angle_rad: f64) -> f64 {
    let n = u.len();
    assert_eq!(n, w.len());
    let mut a = 0.0;
    let mut b = 0.0;
    for i in 0..n {
        a += u[i].x * w[i].x + u[i].y * w[i].y;
        b += u[i].x * w[i].y - u[i].y * w[i].x;
    }
    let theta = b.atan2(a);
    if theta.abs() <= max_angle_rad {
        (a * a + b * b).sqrt()
    } else {
        let phi = if theta > 0.0 { max_angle_rad } else { -max_angle_rad };
        a * phi.cos() + b * phi.sin()
    }
}

fn perpendicular_distance(p: Point2D, line_start: Point2D, line_end: Point2D) -> f64 {
    let dx = line_end.x - line_start.x;
    let dy = line_end.y - line_start.y;
    let mag2 = dx * dx + dy * dy;
    if mag2 < 1e-9 {
        let dx2 = p.x - line_start.x;
        let dy2 = p.y - line_start.y;
        return (dx2 * dx2 + dy2 * dy2).sqrt();
    }
    let u = ((p.x - line_start.x) * dx + (p.y - line_start.y) * dy) / mag2;
    let u = u.clamp(0.0, 1.0);
    let intersection_x = line_start.x + u * dx;
    let intersection_y = line_start.y + u * dy;
    let diff_x = p.x - intersection_x;
    let diff_y = p.y - intersection_y;
    (diff_x * diff_x + diff_y * diff_y).sqrt()
}

fn douglas_peucker_recursive(points: &[Point2D], start: usize, end: usize, epsilon: f64, keep: &mut [bool]) {
    if end <= start + 1 {
        return;
    }
    let mut max_dist = 0.0;
    let mut index = start;
    for i in (start + 1)..end {
        let dist = perpendicular_distance(points[i], points[start], points[end]);
        if dist > max_dist {
            max_dist = dist;
            index = i;
        }
    }
    if max_dist > epsilon {
        keep[index] = true;
        douglas_peucker_recursive(points, start, index, epsilon, keep);
        douglas_peucker_recursive(points, index, end, epsilon, keep);
    }
}

pub fn simplify_points(points: &[Point2D], epsilon: f64) -> Vec<Point2D> {
    if points.len() <= 2 {
        return points.to_vec();
    }
    let mut keep = vec![false; points.len()];
    keep[0] = true;
    keep[points.len() - 1] = true;
    douglas_peucker_recursive(points, 0, points.len() - 1, epsilon, &mut keep);
    
    points.iter().enumerate()
        .filter(|(idx, _)| keep[*idx])
        .map(|(_, &p)| p)
        .collect()
}

pub fn match_gesture(
    captured_points: &[Point2D],
    templates: &[(String, Vec<Point2D>)],
) -> Option<String> {
    if captured_points.len() < 2 {
        return None;
    }
    let n = 64;
    let threshold_score = 0.80;
    let max_angle = 15.0 * (std::f64::consts::PI / 180.0);

    let mut input_vector = resample_path(captured_points, n);
    vectorize_path(&mut input_vector);

    let mut best_name = None;
    let mut max_score = -1.0;

    for (name, template_points) in templates {
        if template_points.len() < 2 {
            continue;
        }
        let mut temp_vector = resample_path(template_points, n);
        vectorize_path(&mut temp_vector);

        let score = compute_protractor_similarity(&input_vector, &temp_vector, max_angle);
        if score > max_score {
            max_score = score;
            best_name = Some(name.clone());
        }
    }

    if max_score >= threshold_score {
        best_name
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_path_length() {
        let pts = vec![
            Point2D { x: 0.0, y: 0.0 },
            Point2D { x: 3.0, y: 4.0 },
        ];
        assert_eq!(path_length(&pts), 5.0);
    }

    #[test]
    fn test_resample_path() {
        let pts = vec![
            Point2D { x: 0.0, y: 0.0 },
            Point2D { x: 10.0, y: 0.0 },
        ];
        let resampled = resample_path(&pts, 5);
        assert_eq!(resampled.len(), 5);
        assert_eq!(resampled[0], Point2D { x: 0.0, y: 0.0 });
        assert_eq!(resampled[1], Point2D { x: 2.5, y: 0.0 });
        assert_eq!(resampled[2], Point2D { x: 5.0, y: 0.0 });
        assert_eq!(resampled[3], Point2D { x: 7.5, y: 0.0 });
        assert_eq!(resampled[4], Point2D { x: 10.0, y: 0.0 });
    }
}
