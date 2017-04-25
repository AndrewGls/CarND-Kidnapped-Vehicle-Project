/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <limits>
#include <assert.h>

#include "helper_functions.h"
#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of 
	//   x, y, theta and their uncertainties from GPS) and all weights to 1. 
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).

	particles.resize(num_particles);
	weights.resize(num_particles, 1.);

	default_random_engine gen;
	normal_distribution<double> dist_x(x, std[0]);
	normal_distribution<double> dist_y(y, std[1]);
	normal_distribution<double> dist_theta(theta, std[2]);

	for (int i = 0; i < num_particles; ++i) {
		Particle& pa = particles[i];
		pa.id = 0;
		pa.x = dist_x(gen);
		pa.y = dist_y(gen);
		pa.theta = dist_theta(gen);
		pa.weight = 1.;
	}

	is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/

	const double epsilon = std::numeric_limits<double>::epsilon();

	default_random_engine gen;
	normal_distribution<double> N_x(0, std_pos[0]);
	normal_distribution<double> N_y(0, std_pos[1]);
	normal_distribution<double> N_yaw(0, std_pos[2]);

	for (int i = 0; i < num_particles; ++i)
	{
		Particle& pa = particles[i];

		if (FP_ZERO == fpclassify(yaw_rate)) {
			// zero yaw rate
			assert(false);
			pa.x += velocity * delta_t * cos(pa.theta) + N_x(gen);
			pa.y += velocity * delta_t * sin(pa.theta) + N_y(gen);
		}
		else {
			// non-zero yaw rate
			double curr_theta = pa.theta;
			pa.theta += yaw_rate * delta_t + N_yaw(gen);
			pa.x += velocity * (sin(pa.theta  ) - sin(curr_theta)) / yaw_rate + N_x(gen);
			pa.y += velocity * (cos(curr_theta) - cos(pa.theta  )) / yaw_rate + N_y(gen);
		}
	}
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the 
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
	//   implement this method and use it as a helper during the updateWeights phase.


}

namespace
{
	inline void TransfromObservationFromCarSpaceToMapSpace(const Particle& particle, LandmarkObs& observ)
	{
		// |x'| |cos(a) -sin(a) tx| |x|
		// |y'|=|sin(a)  cos(a) ty|*|y|
		// |1 | | 0        0     0| |1|

		const double car_obs_x = observ.x;
		const double car_obs_y = observ.y;
		observ.x = cos(particle.theta) * car_obs_x - sin(particle.theta) * car_obs_y + particle.x;
		observ.y = sin(particle.theta) * car_obs_x + cos(particle.theta) * car_obs_y + particle.y;
	}

	inline double CalcNormProb(const LandmarkObs& predicted, const LandmarkObs& observation, double sigma_x, double sigma_y)
	{
		double dx = observation.x - predicted.x;
		double dy = observation.y - predicted.y;
		return exp(-dx*dx / (2 * sigma_x*sigma_x) - dy*dy / (2 * sigma_y*sigma_y)) / (2 * M_PI * sigma_x * sigma_y);
	}

	void FindAssociation(std::vector<LandmarkObs>& predicted, const std::vector<LandmarkObs>& observations, const Map& map_landmarks)
	{
		predicted.resize(observations.size());

		if (observations.empty())
			return;

		std::vector<int> min_dist(observations.size(), INT_MAX);
		const vector<Map::single_landmark_s>& landmarks = map_landmarks.landmark_list;

		for (int i = 0; i < landmarks.size(); i++)
		{
			const Map::single_landmark_s& lm = landmarks[i];

			for (int j = 0; j < observations.size(); j++)
			{
				// find nearest landmark to observation
				const LandmarkObs& obs = observations[j];
				double distance = dist(lm.x_f, lm.y_f, obs.x, obs.y);
				if (distance < min_dist[j])
				{
					min_dist[j] = distance;

					LandmarkObs& pred = predicted[j];
					pred.id = lm.id_i;
					pred.x = lm.x_f;
					pred.y = lm.y_f;
				}
			}
		}
	}
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
		std::vector<LandmarkObs> observations, Map map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation 
	//   3.33. Note that you'll need to switch the minus sign in that equation to a plus to account 
	//   for the fact that the map's y-axis actually points downwards.)
	//   http://planning.cs.uiuc.edu/node99.html

	std::vector<LandmarkObs> predicted;

	for (int i = 0; i < num_particles; ++i)
	{
		Particle& pa = particles[i];

		// transform observations from car space to map space using particle position and orientation.
		TransfromObservationFromCarSpaceToMapSpace(pa, observations[i]);

		// find nearest landmarks to observations
		FindAssociation(predicted, observations, map_landmarks);

		// update weight of particle using calculated Multivariate-Gaussian probability
		double prob = 1.;
		for (int i = 0; i < predicted.size(); i++) {
			const LandmarkObs& pred = predicted[i];
			prob *= CalcNormProb(predicted[i], observations[i], );
		}
		pa.weight *= prob;
	}
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight. 
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution

}

void ParticleFilter::write(std::string filename) {
	// You don't need to modify this file.
	std::ofstream dataFile;
	dataFile.open(filename, std::ios::app);
	for (int i = 0; i < num_particles; ++i) {
		dataFile << particles[i].x << " " << particles[i].y << " " << particles[i].theta << "\n";
	}
	dataFile.close();
}